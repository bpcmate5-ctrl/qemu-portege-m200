#!/usr/bin/env python3
import tempfile
import unittest
from pathlib import Path

import decode_lpc
import make_test_capture


class DecoderTest(unittest.TestCase):
    def test_reference_vector(self):
        operations = [
            (True, 0x66, 0x9e), (True, 0x62, 0x11),
            (True, 0x62, 0x10), (True, 0x66, 0x9f),
            (False, 0x66, 0x71), (False, 0x62, 0xa5),
            (False, 0x62, 0x5a),
        ]
        samples = []
        for op in operations:
            samples.extend(make_test_capture.io_cycle(*op))
        decoded_samples = [
            decode_lpc.Sample(i, x & 15, (x >> 4) & 1, (x >> 5) & 1, x)
            for i, x in enumerate(samples)
        ]
        cycles, errors = decode_lpc.decode(decoded_samples)
        self.assertEqual(errors, [])
        self.assertEqual(
            [(c.direction, c.address, c.data) for c in cycles],
            [("WRITE" if w else "READ", a, d) for w, a, d in operations],
        )

    def test_malformed_is_visible(self):
        raw = [make_test_capture.sample(0, 0), make_test_capture.sample(7)]
        samples = [
            decode_lpc.Sample(i, x & 15, (x >> 4) & 1, (x >> 5) & 1, x)
            for i, x in enumerate(raw)
        ]
        cycles, errors = decode_lpc.decode(samples)
        self.assertEqual(cycles, [])
        self.assertEqual(len(errors), 1)

    def test_two_phase_file(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture.bin"
            old_argv = make_test_capture.sys.argv
            try:
                make_test_capture.sys.argv = ["make_test_capture.py", str(output)]
                make_test_capture.main()
            finally:
                make_test_capture.sys.argv = old_argv
            meta, samples = decode_lpc.load_capture(output)
            cycles, errors = decode_lpc.decode(samples)
        self.assertTrue(meta["flags"] & decode_lpc.FLAG_TWO_PHASE)
        self.assertEqual(errors, [])
        self.assertEqual(len(cycles), 7)

    def test_two_phase_bad_clock_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture.bin"
            old_argv = make_test_capture.sys.argv
            try:
                make_test_capture.sys.argv = ["make_test_capture.py", str(output)]
                make_test_capture.main()
            finally:
                make_test_capture.sys.argv = old_argv
            data = bytearray(output.read_bytes())
            data[decode_lpc.HEADER.size + 1] &= ~0x20
            output.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "bad low/high phase pair"):
                decode_lpc.load_capture(output)

    def test_long_idle_and_back_to_back_cycles(self):
        logical = [make_test_capture.sample(15)] * 10000
        logical += make_test_capture.io_cycle(True, 0x66, 0x9e)
        logical += make_test_capture.io_cycle(False, 0x62, 0xa5)
        samples = [
            decode_lpc.Sample(i, x & 15, (x >> 4) & 1, (x >> 5) & 1, x)
            for i, x in enumerate(logical)
        ]
        cycles, errors = decode_lpc.decode(samples)
        self.assertEqual(errors, [])
        self.assertEqual(
            [(c.direction, c.address, c.data) for c in cycles],
            [("WRITE", 0x66, 0x9e), ("READ", 0x62, 0xa5)],
        )


if __name__ == "__main__":
    unittest.main()
