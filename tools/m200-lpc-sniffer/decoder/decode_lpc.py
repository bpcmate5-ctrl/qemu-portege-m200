#!/usr/bin/env python3
"""Decode generic LPC I/O cycles from M200 RP2040 raw captures."""

from __future__ import annotations

import argparse
import dataclasses
import json
import struct
import sys
from pathlib import Path

MAGIC = b"M2LPRAW\x00"
HEADER = struct.Struct("<8sHH13I")
FLAG_TWO_PHASE = 1 << 3


@dataclasses.dataclass
class Sample:
    ordinal: int
    lad: int
    frame_n: int
    lclk: int
    raw: int


@dataclasses.dataclass
class Cycle:
    sample: int
    direction: str
    address: int
    data: int
    sync: int
    raw_nibbles: list[int]
    trailing_samples: int


def load_capture(path: Path) -> tuple[dict, list[Sample]]:
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise ValueError("capture is shorter than the 64-byte header")
    fields = HEADER.unpack_from(data)
    if fields[0] != MAGIC:
        raise ValueError(f"bad capture magic {fields[0]!r}")
    version, header_bytes = fields[1:3]
    if version != 1 or header_bytes < HEADER.size:
        raise ValueError(f"unsupported header version/size {version}/{header_bytes}")
    values = fields[3:]
    keys = (
        "flags", "sys_hz", "expected_lclk_hz", "sample_count",
        "payload_bytes", "input_gpio_base", "lclk_gpio", "lframe_gpio",
        "reserved0", "reserved1", "reserved2", "reserved3", "reserved4",
    )
    meta = dict(zip(keys, values))
    payload = data[header_bytes:header_bytes + meta["payload_bytes"]]
    if len(payload) != meta["payload_bytes"]:
        raise ValueError("truncated capture payload")
    if meta["sample_count"] > len(payload):
        raise ValueError("sample count exceeds byte payload")
    raw_samples = [
        Sample(i, byte & 0x0F, (byte >> 4) & 1, (byte >> 5) & 1, byte)
        for i, byte in enumerate(payload[:meta["sample_count"]])
    ]
    if meta["flags"] & FLAG_TWO_PHASE:
        if len(raw_samples) & 1:
            raise ValueError("two-phase capture has an odd raw sample count")
        samples = []
        for index in range(0, len(raw_samples), 2):
            low, high = raw_samples[index:index + 2]
            if low.lclk != 0 or high.lclk != 1:
                raise ValueError(
                    f"bad low/high phase pair at raw sample {index}: "
                    f"LCLK={low.lclk}/{high.lclk}")
            # LPC defines the rising edge as the sampling edge.  Retain the
            # low-phase value immediately preceding it as the protocol sample.
            samples.append(Sample(index // 2, low.lad, low.frame_n, 1, low.raw))
    else:
        samples = raw_samples
    return meta, samples


def decode_frame(samples: list[Sample], start: int, end: int) -> tuple[Cycle | None, str | None]:
    frame = samples[start:end]
    if len(frame) < 13:
        return None, f"short frame: {len(frame)} samples"
    n = [s.lad for s in frame[:13]]
    if n[0] != 0:
        return None, f"bad START nibble {n[0]:x}"
    cycle_class = n[1] & 0xE
    if cycle_class == 0x2:
        direction = "WRITE"
    elif cycle_class == 0x0:
        direction = "READ"
    else:
        return None, f"unsupported CYCTYPE/DIR nibble {n[1]:x}"
    address = (n[2] << 12) | (n[3] << 8) | (n[4] << 4) | n[5]
    if direction == "WRITE":
        data = n[6] | (n[7] << 4)
        sync = n[10]
        tar_ok = n[8] == n[9] == n[11] == n[12] == 0xF
    else:
        sync = n[8]
        data = n[9] | (n[10] << 4)
        tar_ok = n[6] == n[7] == n[11] == n[12] == 0xF
    if not tar_ok:
        return None, "turnaround field is not 0xf"
    if sync != 0:
        return None, f"non-ready/extended SYNC {sync:x}; raw frame retained"
    return Cycle(start, direction, address, data, sync, n, len(frame) - 13), None


def decode(samples: list[Sample]) -> tuple[list[Cycle], list[dict]]:
    starts = [i for i, sample in enumerate(samples) if sample.frame_n == 0]
    cycles: list[Cycle] = []
    errors: list[dict] = []
    for number, start in enumerate(starts):
        end = starts[number + 1] if number + 1 < len(starts) else len(samples)
        cycle, error = decode_frame(samples, start, end)
        if cycle:
            cycles.append(cycle)
        else:
            errors.append({
                "sample": start,
                "end": end,
                "error": error,
                "raw": [s.raw for s in samples[start:end]],
            })
    return cycles, errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--ports", nargs="*", type=lambda x: int(x, 0))
    args = parser.parse_args()
    try:
        meta, samples = load_capture(args.capture)
        cycles, errors = decode(samples)
    except (OSError, ValueError) as exc:
        print(f"decode_lpc: {exc}", file=sys.stderr)
        return 2
    if args.ports:
        cycles = [cycle for cycle in cycles if cycle.address in args.ports]
    if args.json:
        print(json.dumps({
            "metadata": meta,
            "cycles": [dataclasses.asdict(c) for c in cycles],
            "errors": errors,
        }, indent=2))
    else:
        for cycle in cycles:
            arrow = "<-" if cycle.direction == "WRITE" else "->"
            print(f"{cycle.sample:10d} IO {cycle.direction:5s} "
                  f"{cycle.address:04X} {arrow} {cycle.data:02X}")
        for error in errors:
            print(f"{error['sample']:10d} UNDECODED {error['error']}",
                  file=sys.stderr)
        print(f"# {len(cycles)} decoded, {len(errors)} undecoded", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
