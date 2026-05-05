Import("env")

import hashlib
import json
from pathlib import Path


def _parse_model_flag(build_flags):
    for flag in build_flags:
        if isinstance(flag, str) and flag.startswith("-DModel="):
            return flag.split("=", 1)[1]
    return None


def _replace_model_flag(build_flags, new_value):
    replaced = False
    out = []
    for flag in build_flags:
        if isinstance(flag, str) and flag.startswith("-DModel="):
            out.append("-DModel=%s" % new_value)
            replaced = True
        else:
            out.append(flag)
    if not replaced:
        out.append("-DModel=%s" % new_value)
    return out


def _sha256(path_obj):
    h = hashlib.sha256()
    with open(path_obj, "rb") as f:
        while True:
            chunk = f.read(8192)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def _size_limit_bytes(kib_value):
    return int(kib_value) * 1024


def _append_flash_images(images):
    env.Append(FLASH_EXTRA_IMAGES=images)


def _framework_partition_file(rel_path):
    fw = Path(env.PioPlatform().get_package_dir("framework-arduinounihiker"))
    return fw / rel_path


def _append_en_fallback(reason):
    en_model = _framework_partition_file("tools/partitions/srmodels4.bin")
    en_voice = _framework_partition_file("tools/partitions/esp_tts_voice_data_xiaoxin.dat")

    print("[PTBR] WARNING: %s" % reason)
    print("[PTBR] Fallback -> EN model bundle")

    missing = []
    if not en_model.is_file():
        missing.append(str(en_model))
    if not en_voice.is_file():
        missing.append(str(en_voice))
    if missing:
        raise Exception("[PTBR] EN fallback artifacts missing: %s" % ", ".join(missing))

    _append_flash_images([
        ("0x510000", str(en_model)),
        ("0x985000", str(en_voice)),
    ])

    env.Append(CPPDEFINES=[
        ("UNIHIKER_PRO_SPEECH_MODEL", '\"EN\"'),
        "UNIHIKER_PRO_SPEECH_MODEL_FALLBACK_EN",
    ])


def _manifest_error(policy, message):
    if policy == "error":
        raise Exception("[PTBR] %s" % message)
    _append_en_fallback(message)


def _load_manifest(manifest_path):
    with open(manifest_path, "r", encoding="utf-8") as f:
        return json.load(f)


def _validate_and_append_ptbr(manifest_path, manifest, policy):
    required_top = ["flash_offsets", "artifacts"]
    for key in required_top:
        if key not in manifest:
            _manifest_error(policy, "Manifest missing key '%s'" % key)
            return

    offsets = manifest.get("flash_offsets", {})
    artifacts = manifest.get("artifacts", {})
    checksums = manifest.get("checksums", {})

    model_offset = offsets.get("model", "0x510000")
    voice_offset = offsets.get("voice_data", "0x985000")

    model_file = artifacts.get("model")
    voice_file = artifacts.get("voice_data")

    if not model_file or not voice_file:
        _manifest_error(policy, "Manifest artifacts.model/artifacts.voice_data are required")
        return

    base_dir = manifest_path.parent
    model_path = (base_dir / model_file).resolve()
    voice_path = (base_dir / voice_file).resolve()

    if not model_path.is_file() or not voice_path.is_file():
        missing = []
        if not model_path.is_file():
            missing.append(str(model_path))
        if not voice_path.is_file():
            missing.append(str(voice_path))
        _manifest_error(policy, "PTBR artifact(s) not found: %s" % ", ".join(missing))
        return

    model_max = _size_limit_bytes(4563)
    voice_max = _size_limit_bytes(2542)

    model_size = model_path.stat().st_size
    voice_size = voice_path.stat().st_size

    if model_size > model_max:
        _manifest_error(policy, "model artifact too large (%d > %d bytes)" % (model_size, model_max))
        return
    if voice_size > voice_max:
        _manifest_error(policy, "voice_data artifact too large (%d > %d bytes)" % (voice_size, voice_max))
        return

    expected_model_sha = checksums.get("model", "").strip().lower() if isinstance(checksums.get("model", ""), str) else ""
    expected_voice_sha = checksums.get("voice_data", "").strip().lower() if isinstance(checksums.get("voice_data", ""), str) else ""

    if expected_model_sha:
        model_sha = _sha256(model_path)
        if model_sha != expected_model_sha:
            _manifest_error(policy, "model checksum mismatch")
            return

    if expected_voice_sha:
        voice_sha = _sha256(voice_path)
        if voice_sha != expected_voice_sha:
            _manifest_error(policy, "voice_data checksum mismatch")
            return

    _append_flash_images([
        (model_offset, str(model_path)),
        (voice_offset, str(voice_path)),
    ])

    env.Append(CPPDEFINES=[
        ("UNIHIKER_PRO_SPEECH_MODEL", '\"PTBR\"'),
    ])

    print("[PTBR] PTBR model artifacts added from manifest: %s" % manifest_path)


def _main():
    build_flags = list(env.get("BUILD_FLAGS", []))
    selected = _parse_model_flag(build_flags)

    if selected != "PTBR":
        return

    # External framework builder only supports CN/EN and would warn for PTBR.
    # Keep Model=None there and inject PTBR images from this project script.
    env.Replace(BUILD_FLAGS=_replace_model_flag(build_flags, "None"))

    policy = env.GetProjectOption(
        "custom_ptbr_model_policy",
        env.GetProjectOption("ptbr_model_policy", "fallback_en"),
    ).strip().lower()
    if policy not in ("fallback_en", "error"):
        policy = "fallback_en"

    manifest_opt = env.GetProjectOption(
        "custom_ptbr_model_manifest",
        env.GetProjectOption(
            "ptbr_model_manifest",
            "../../assets/speech-models/ptbr/v1/manifest.json",
        ),
    )
    manifest_path = (Path(env.subst("$PROJECT_DIR")) / manifest_opt).resolve()

    if not manifest_path.is_file():
        _manifest_error(policy, "PTBR manifest not found: %s" % manifest_path)
        return

    try:
        manifest = _load_manifest(manifest_path)
    except Exception as exc:
        _manifest_error(policy, "Failed to parse manifest (%s)" % exc)
        return

    _validate_and_append_ptbr(manifest_path, manifest, policy)


_main()
