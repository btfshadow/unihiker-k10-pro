#!/usr/bin/env python3
"""Convert TTF/OTF fonts into LVGL binary font files (.bin).

This utility is intended for UNIHIKER K10 display usage where runtime TTF is
not available in the current SDK build. It generates an LVGL-compatible .bin
font that can be loaded at runtime with DisplayService::loadFontFile().

Examples:
  python scripts/ttf_to_lvgl_bin.py --input assets/fonts/NotoSans-Regular.ttf
  python scripts/ttf_to_lvgl_bin.py --input MyFont.ttf --size 24 --bpp 3 \
      --profile ptbr-ext --output out/MyFont24.bin
"""

from __future__ import annotations

import argparse
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


PROFILE_RANGES = {
    "ascii": "0x20-0x7E",
    "ptbr": "0x20-0x7E,0xA0-0xFF",
    "ptbr-ext": "0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026",
}


def _resolve_converter(explicit: str | None) -> list[str]:
    if explicit:
        return shlex.split(explicit)

    direct = shutil.which("lv_font_conv")
    if direct:
        return [direct]

    npx = shutil.which("npx")
    if npx:
        return [npx, "-y", "lv_font_conv"]

    raise RuntimeError(
        "lv_font_conv not found. Install Node.js and run: npm i -g lv_font_conv"
    )


def _default_output(input_path: Path, size: int, bpp: int) -> Path:
    return input_path.with_suffix("").with_name(f"{input_path.stem}_{size}px_bpp{bpp}.bin")


def _validate_input(path: Path) -> None:
    if not path.is_file():
        raise RuntimeError(f"input font not found: {path}")
    if path.suffix.lower() not in (".ttf", ".otf", ".woff", ".woff2"):
        raise RuntimeError("input must be .ttf, .otf, .woff, or .woff2")


def _build_range(profile: str, custom: str | None, extra: str | None) -> str:
    base = custom if custom else PROFILE_RANGES[profile]
    if extra:
        return f"{base},{extra}"
    return base


def _build_command(
    converter: list[str],
    input_path: Path,
    output_path: Path,
    size: int,
    bpp: int,
    range_expr: str,
    compress: bool,
) -> list[str]:
    cmd = list(converter)
    cmd += [
        "--bpp",
        str(bpp),
        "--size",
        str(size),
        "--font",
        str(input_path),
        "-r",
        range_expr,
        "--format",
        "bin",
        "-o",
        str(output_path),
        "--force-fast-kern-format",
        "--no-prefilter",
    ]
    if not compress:
        cmd.append("--no-compress")
    return cmd


def _run(cmd: list[str], dry_run: bool) -> int:
    print("[font-util] command:")
    print("  " + " ".join(shlex.quote(c) for c in cmd))

    if dry_run:
        print("[font-util] dry-run enabled, conversion not executed")
        return 0

    proc = subprocess.run(cmd, check=False)
    return proc.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert TTF/OTF to LVGL .bin")
    parser.add_argument("--input", required=True, type=Path, help="input font file")
    parser.add_argument("--output", type=Path, help="output .bin file path")
    parser.add_argument("--size", type=int, default=24, help="font size in px (default: 24)")
    parser.add_argument("--bpp", type=int, default=3, choices=[1, 2, 3, 4], help="bits per pixel")
    parser.add_argument(
        "--profile",
        choices=sorted(PROFILE_RANGES.keys()),
        default="ptbr",
        help="preset Unicode profile",
    )
    parser.add_argument(
        "--range",
        dest="custom_range",
        help="custom unicode range expression (overrides --profile)",
    )
    parser.add_argument("--extra-range", help="extra range expression appended to base range")
    parser.add_argument(
        "--compress",
        action="store_true",
        help="enable glyph compression (default: disabled)",
    )
    parser.add_argument("--converter", help="explicit converter command, e.g. 'npx -y lv_font_conv'")
    parser.add_argument("--dry-run", action="store_true", help="print command only")
    args = parser.parse_args()

    try:
        _validate_input(args.input)
        converter = _resolve_converter(args.converter)
        output = args.output or _default_output(args.input, args.size, args.bpp)
        output = output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)

        range_expr = _build_range(args.profile, args.custom_range, args.extra_range)
        cmd = _build_command(
            converter=converter,
            input_path=args.input.resolve(),
            output_path=output,
            size=args.size,
            bpp=args.bpp,
            range_expr=range_expr,
            compress=args.compress,
        )

        rc = _run(cmd, args.dry_run)
        if rc != 0:
            print(f"[font-util] conversion failed with exit code {rc}")
            return rc

        if not args.dry_run:
            print(f"[font-util] output: {output}")
            print("[font-util] device usage:")
            print(f"  font load S:/fonts/{output.name}")
            print("[font-util] note: .ttf/.otf input on device is alias to same-name .bin")
        return 0

    except RuntimeError as exc:
        print(f"[font-util] error: {exc}")
        return 2


if __name__ == "__main__":
    sys.exit(main())
