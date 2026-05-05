#!/usr/bin/env python3
"""Provision PTBR speech artifacts into unihiker-pro assets and update manifest checksums.

Usage example:
  python scripts/provision_ptbr_artifacts.py \
    --model /path/to/srmodels_ptbr.bin \
    --voice /path/to/tts_voice_ptbr.dat
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path


MODEL_MAX_BYTES = 4563 * 1024
VOICE_MAX_BYTES = 2542 * 1024
DEST_MODEL_NAME = "srmodels_ptbr.bin"
DEST_VOICE_NAME = "tts_voice_ptbr.dat"

# Known legacy framework artifacts. Reject by default so we do not ship fake PTBR.
KNOWN_LEGACY_SHA256 = {
    "38e9b67b1769ddbae421120a5f68692291c8bbc27f9f1c85a62eccdf40917150": "srmodels.bin (CN)",
    "3c87330e5ad21ad8b9542fbb97388a382b6028ce7d3f2df35f14f268db676830": "srmodels4.bin (EN)",
    "2b265c85b0f4d45d12d54c46ef2042a1662905a4a4e97a0035f4796419620a73": "srmodels5.bin (unknown mix)",
    "673881f05338b27cee7f071aa06121b3dacb0ecbe71dc80c31e57c092bbf30d3": "esp_tts_voice_data_xiaoxin.dat (legacy voice)",
}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            chunk = f.read(8192)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def require_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise SystemExit(f"[PTBR] Missing {label} file: {path}")


def verify_size(path: Path, max_bytes: int, label: str) -> None:
    size = path.stat().st_size
    if size > max_bytes:
        raise SystemExit(
            f"[PTBR] {label} file too large ({size} > {max_bytes} bytes): {path}"
        )


def reject_known_legacy(hash_value: str, label: str, allow_legacy: bool) -> None:
    legacy_name = KNOWN_LEGACY_SHA256.get(hash_value)
    if legacy_name and not allow_legacy:
        raise SystemExit(
            f"[PTBR] {label} looks like legacy artifact '{legacy_name}' (sha={hash_value}). "
            "Refusing by default. Use --allow-known-legacy only for controlled diagnostics."
        )


def load_manifest(path: Path) -> dict:
    if not path.is_file():
        raise SystemExit(f"[PTBR] Manifest not found: {path}")
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def save_manifest(path: Path, data: dict) -> None:
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=True)
        f.write("\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Provision PTBR speech model artifacts")
    parser.add_argument("--model", required=True, type=Path, help="Path to PTBR ASR model binary")
    parser.add_argument("--voice", required=True, type=Path, help="Path to PTBR TTS voice data")
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("unihiker-pro/assets/speech-models/ptbr/v1/manifest.json"),
        help="Manifest path to update",
    )
    parser.add_argument(
        "--allow-known-legacy",
        action="store_true",
        help="Allow known legacy CN/EN hashes (diagnostics only)",
    )
    args = parser.parse_args()

    model_src = args.model.resolve()
    voice_src = args.voice.resolve()
    manifest_path = args.manifest.resolve()
    dest_dir = manifest_path.parent

    require_file(model_src, "model")
    require_file(voice_src, "voice")

    verify_size(model_src, MODEL_MAX_BYTES, "model")
    verify_size(voice_src, VOICE_MAX_BYTES, "voice")

    model_sha = sha256_file(model_src)
    voice_sha = sha256_file(voice_src)

    reject_known_legacy(model_sha, "model", args.allow_known_legacy)
    reject_known_legacy(voice_sha, "voice", args.allow_known_legacy)

    manifest = load_manifest(manifest_path)

    dest_dir.mkdir(parents=True, exist_ok=True)
    model_dst = dest_dir / DEST_MODEL_NAME
    voice_dst = dest_dir / DEST_VOICE_NAME

    shutil.copy2(model_src, model_dst)
    shutil.copy2(voice_src, voice_dst)

    manifest.setdefault("artifacts", {})["model"] = DEST_MODEL_NAME
    manifest.setdefault("artifacts", {})["voice_data"] = DEST_VOICE_NAME
    manifest.setdefault("checksums", {})["model"] = sha256_file(model_dst)
    manifest.setdefault("checksums", {})["voice_data"] = sha256_file(voice_dst)

    save_manifest(manifest_path, manifest)

    print("[PTBR] Provision completed")
    print(f"[PTBR] model -> {model_dst} ({model_dst.stat().st_size} bytes)")
    print(f"[PTBR] voice -> {voice_dst} ({voice_dst.stat().st_size} bytes)")
    print(f"[PTBR] manifest -> {manifest_path}")
    print(f"[PTBR] checksum model={manifest['checksums']['model']}")
    print(f"[PTBR] checksum voice={manifest['checksums']['voice_data']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
