#!/usr/bin/env python3
"""Create a software reference capture matching the bench generator vector."""

import argparse
import struct
import sys
from pathlib import Path

MAGIC = b"M2LPRAW\x00"
HEADER = struct.Struct("<8sHH13I")


def sample(lad: int, frame_n: int = 1) -> int:
    return (lad & 15) | ((frame_n & 1) << 4) | 0x20


def io_cycle(write: bool, address: int, data: int) -> list[int]:
    n = [sample(0, 0), sample(2 if write else 0)]
    n += [sample((address >> shift) & 15) for shift in (12, 8, 4, 0)]
    if write:
        n += [sample(data & 15), sample(data >> 4), sample(15), sample(15), sample(0)]
    else:
        n += [sample(15), sample(15), sample(0), sample(data & 15), sample(data >> 4)]
    n += [sample(15), sample(15)]
    n += [sample(15)] * 3
    return n


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("output", type=Path)
    ap.add_argument("--malformed", action="store_true")
    args = ap.parse_args()
    operations = [
        (True, 0x66, 0x9e), (True, 0x62, 0x11),
        (True, 0x62, 0x10), (True, 0x66, 0x9f),
        (False, 0x66, 0x71), (False, 0x62, 0xa5),
        (False, 0x62, 0x5a),
    ]
    logical = bytearray()
    for op in operations:
        logical.extend(io_cycle(*op))
    if args.malformed:
        logical.extend((sample(0, 0), sample(7), sample(0)))
    payload = bytearray()
    for value in logical:
        payload.extend((value & ~0x20, value | 0x20))
    fields = [
        1 << 3, 133_000_000, 33_000_000, len(payload), len(payload),
        2, 7, 6, 0, 0, 0, 0, 0,
    ]
    args.output.write_bytes(HEADER.pack(MAGIC, 1, HEADER.size, *fields) + payload)


if __name__ == "__main__":
    main()
