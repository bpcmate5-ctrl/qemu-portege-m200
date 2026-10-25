# M200 RP2040 LPC capture tools

This directory contains an experimental, bench-only passive LPC capture
design.  It has not been connected to a Toshiba M200.

## Layout

- `pico/`: one-RP2040 PIO/DMA sniffer.
- `generator/`: second-RP2040 deterministic bench waveform generator. Never
  connect generator pins to a motherboard.
- `decoder/`: binary acquisition and generic LPC I/O-cycle decoder.
- `common/`: versioned capture-file format.

The sniffer uses contiguous GPIO2..GPIO9: LAD0..3, LFRAME#, LCLK, and two
reserved inputs. Its PIO program contains only `wait` and `in` instructions;
it has no output or pin-direction instruction. Each LPC rising edge is
bracketed by two samples. The low-phase sample supplies the protocol value and
the high-phase sample validates clock alignment.

## Build

Set `PICO_SDK_PATH` to a Pico SDK checkout, then build each target out of tree:

```sh
cmake -S pico -B /tmp/m200-lpc-sniffer-build -G Ninja
ninja -C /tmp/m200-lpc-sniffer-build
cmake -S generator -B /tmp/m200-lpc-generator-build -G Ninja
ninja -C /tmp/m200-lpc-generator-build
```

## Software decoder check

```sh
cd decoder
python3 -m unittest -v test_decoder.py
python3 make_test_capture.py /tmp/m200-lpc-reference.bin
python3 decode_lpc.py /tmp/m200-lpc-reference.bin --ports 0x62 0x66
```

`capture_serial.py` requires pyserial and saves the complete header and raw
payload. Do not pipe binary output through an interactive terminal.

## Bench-validation boundary

The generator produces generic immediate-SYNC LPC I/O cycles containing the
known test bytes plus arbitrary read values. Those read values are test data,
not Toshiba EC responses. A successful software decode is not electrical or
timing validation. Approval for an M200 connection requires a separate review
after Pico-to-Pico testing and independent oscilloscope comparison.
