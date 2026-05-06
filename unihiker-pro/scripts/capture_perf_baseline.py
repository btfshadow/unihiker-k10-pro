#!/usr/bin/env python3
"""Capture structured PERF logs from perf_baseline_smoke and export CSV/JSON.

Usage example:
  /path/to/.venv/bin/python unihiker-pro/scripts/capture_perf_baseline.py \
      --port /dev/cu.usbmodem1201 --loops 220 --rounds 4
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
import time
from pathlib import Path

import serial


def parse_perf_line(line: str) -> dict[str, str] | None:
  line = line.strip()
  if not line.startswith("PERF|"):
    return None

  payload = line[5:]
  parts = payload.split("|")
  out: dict[str, str] = {}
  for part in parts:
    if "=" not in part:
      continue
    key, value = part.split("=", 1)
    out[key.strip()] = value.strip()

  return out if out else None


def write_json(path: Path, rows: list[dict[str, str]]) -> None:
  path.write_text(json.dumps(rows, indent=2, ensure_ascii=True), encoding="utf-8")


def write_csv(path: Path, rows: list[dict[str, str]]) -> None:
  keys = sorted({k for row in rows for k in row.keys()})
  with path.open("w", newline="", encoding="utf-8") as fp:
    writer = csv.DictWriter(fp, fieldnames=keys)
    writer.writeheader()
    writer.writerows(rows)


def main() -> int:
  parser = argparse.ArgumentParser(description="Capture PERF report from serial")
  parser.add_argument("--port", required=True, help="serial port, e.g. /dev/cu.usbmodem1201")
  parser.add_argument("--baud", type=int, default=115200, help="serial baud rate")
  parser.add_argument("--loops", type=int, default=200, help="display bench loops per round")
  parser.add_argument("--rounds", type=int, default=3, help="number of rounds")
  parser.add_argument("--timeout", type=int, default=120, help="capture timeout in seconds")
  parser.add_argument(
      "--out-dir",
      default="unihiker-pro/tests/perf_baseline_smoke/results",
      help="output directory",
  )
  args = parser.parse_args()

  out_dir = Path(args.out_dir)
  out_dir.mkdir(parents=True, exist_ok=True)

  stamp = time.strftime("%Y%m%d-%H%M%S")
  raw_path = out_dir / f"perf-{stamp}.log"
  csv_path = out_dir / f"perf-{stamp}.csv"
  json_path = out_dir / f"perf-{stamp}.json"

  rows: list[dict[str, str]] = []
  start = time.time()

  try:
    with serial.Serial(args.port, args.baud, timeout=0.25) as ser, raw_path.open("w", encoding="utf-8") as raw:
      time.sleep(0.3)
      ser.reset_input_buffer()

      cmd = f"report {args.loops} {args.rounds}\\n".encode("utf-8")
      ser.write(cmd)

      done = False
      while not done and (time.time() - start) < args.timeout:
        data = ser.readline()
        if not data:
          continue

        line = data.decode("utf-8", "replace").rstrip("\r\n")
        print(line)
        raw.write(line + "\n")

        parsed = parse_perf_line(line)
        if parsed:
          parsed["timestamp_ms"] = str(int((time.time() - start) * 1000))
          rows.append(parsed)
          if parsed.get("kind") == "report" and parsed.get("phase") == "end":
            done = True

      if not done:
        print("capture timeout before report end", file=sys.stderr)

  except serial.SerialException as exc:
    print(f"serial error: {exc}", file=sys.stderr)
    return 2

  if rows:
    write_json(json_path, rows)
    write_csv(csv_path, rows)
    print(f"saved json: {json_path}")
    print(f"saved csv : {csv_path}")
  else:
    print("no PERF rows captured", file=sys.stderr)
    return 3

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
