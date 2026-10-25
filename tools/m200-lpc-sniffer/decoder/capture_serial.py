#!/usr/bin/env python3
"""Arm the Pico sniffer and preserve its binary capture without a terminal."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def read_line(port) -> bytes:
    line = port.readline()
    if not line:
        raise TimeoutError("timed out waiting for sniffer")
    return line


def main() -> int:
    try:
        import serial
    except ImportError:
        print("capture_serial: install pyserial in a disposable environment",
              file=sys.stderr)
        return 2
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="Pico USB CDC device")
    parser.add_argument("output", type=Path)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    with serial.Serial(args.port, timeout=args.timeout) as port:
        port.reset_input_buffer()
        port.write(b"A")
        while True:
            line = read_line(port)
            match = re.fullmatch(rb"M200LPC-BINARY ([0-9]+)\r?\n", line)
            if match:
                break
        remaining = int(match.group(1))
        capture = bytearray()
        while remaining:
            block = port.read(remaining)
            if not block:
                raise TimeoutError("truncated binary capture")
            capture.extend(block)
            remaining -= len(block)
    args.output.write_bytes(capture)
    print(f"wrote {len(capture)} bytes to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
