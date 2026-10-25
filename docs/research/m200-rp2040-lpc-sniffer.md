# HW-EC-003A: RP2040 passive LPC sniffer

## Status

The one-RP2040 PIO/DMA design, a second-Pico bench generator, capture format,
and generic offline decoder are implemented. They build and the decoder passes
synthetic software tests. No Pico or SDS814X HD was available to this execution
environment, so no firmware was flashed and no physical bench or scope test
was performed. Nothing was connected to the M200.

Consequently this milestone is **HW-EC-003A FAIL** under the stated acceptance
criteria: scope-confirmed cycle alignment and sustained physical 33 MHz
operation remain mandatory. This is an honest validation boundary, not an
identified design failure.

## Feasibility and timing budget

Intel's *Low Pin Count Interface Specification*, revision 1.1, defines the
33 MHz-class protocol and rising-edge sampling (sections 2 and 4). Thus a
33 MHz clock has about 30.3 ns per cycle. RP2040's documented maximum system
clock is 133 MHz, giving a 7.519 ns PIO
instruction period. The capture loop is:

1. wait for LCLK low;
2. sample eight contiguous inputs;
3. wait for LCLK high;
4. sample the inputs again.

It consumes exactly four PIO instructions per LCLK when running at 33 MHz:
30.075 ns, equivalent to a theoretical 33.25 MHz ceiling. That margin is only
about 0.76%; physical scope validation is therefore essential. PIO input
synchronizers are bypassed to avoid their two-cycle latency. This reduces
phase ambiguity but makes electrical timing and metastability checks even more
important.

LCLK is a synchronization input, not stored merely as an inferred clock. The
PIO samples GPIO2..GPIO9 simultaneously:

| GPIO | Meaning |
|---:|---|
| 2..5 | LAD0..LAD3 |
| 6 | LFRAME# |
| 7 | LCLK |
| 8..9 | reserved inputs |

One state machine is sufficient. DMA transfers 32-bit FIFO words directly to
a 240 KiB static buffer. At two raw bytes per LCLK the peak stream is 66 MB/s,
within RP2040 SRAM DMA bandwidth but consuming the entire buffer in about
3.724 ms (122,880 LPC clocks). No USB work occurs during capture. A full buffer
sets the overflow/window-exhausted flag; activity after the capture window is
lost rather than silently overwritten.

This duration does not guarantee capture of all power-on LPC traffic. The
present trigger arms at the first observed LFRAME# assertion after the host
sends `A`. A later motherboard experiment therefore needs a separately
approved way to arm close enough to F002, or a future streaming/ring-buffer
revision. Triggering on `9E` inside PIO was deliberately avoided.

## Raw representation and decoder

Every raw byte contains LAD[3:0] in bits 0..3, LFRAME# in bit 4, observed LCLK
in bit 5, and two reserved bits. Two chronological bytes bracket each rising
edge. The decoder rejects a pair unless its observed LCLK is 0 then 1, selects
the pre-rising sample as the protocol value, and decodes standard LPC I/O
cycles generically.

The decoder reconstructs START, CYCTYPE/DIR, four address nibbles, turnaround,
SYNC, and byte data. It does not contain special cases for ports 62h/66h or the
9E/9F sequence. It preserves malformed or unsupported frames as raw samples
and exits nonzero. The initial implementation accepts immediate-ready SYNC;
extended wait-state decoding remains deliberately visible as unsupported.

The deterministic generator emits these generic test operations:

```
IO WRITE 0066 <- 9E
IO WRITE 0062 <- 11
IO WRITE 0062 <- 10
IO WRITE 0066 <- 9F
IO READ  0066 -> 71
IO READ  0062 -> A5
IO READ  0062 -> 5A
```

The read data are arbitrary test vectors and make no claim about the Toshiba
EC response.

## Input-only safety architecture

The sniffer firmware initializes all eight capture pins as inputs, disables
internal pulls, and repeats that setup before arming. Its PIO program has no
`set`, `out`, side-set, or pin-direction operation. DMA only reads the PIO RX
FIFO. This proves the intended firmware path has no drive operation; it does
not by itself make a bare Pico connection safe during reset, boot ROM, faults,
or firmware replacement.

The eventual interface should therefore place a unidirectional,
motherboard-to-Pico, high-input-impedance 3.3 V logic buffer between every LPC
net and RP2040. Its output-enable must be fixed so no path can drive toward the
motherboard, with short leads and verified power-domain behavior. Exact buffer
selection and wiring are intentionally deferred to the separate electrical
interface review. No motherboard attachment instructions are provided here.

Primary references used here are Intel's
[LPC Interface Specification, revision 1.1](https://www.intel.com/content/dam/www/program/design/us/en/documents/low-pin-count-interface-specification.pdf)
and Raspberry Pi's
[RP2040 datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf),
especially its PIO and GPIO chapters. Raspberry Pi specifies 133 MHz and
264 KiB SRAM. The compiled sniffer occupies 38,804 bytes of text and 249,064
bytes of data/BSS, leaving little SRAM headroom; increasing the capture buffer
without redesign is unsafe.

## Required bench validation

Use the second Pico only as a bench generator. It drives the synthetic signals
and returns them to high impedance after the vector. It must never be attached
to a motherboard.

The SDS814X HD is the independent reference. A defensible procedure is:

1. Observe LCLK, LFRAME#, LAD0 and LAD1 while the sniffer records the same
   generator run.
2. Repeat with LCLK, LFRAME#, LAD2 and LAD3.
3. Confirm on the scope that inputs are stable at the rising edge and compare
   every decoded sample/cycle with the waveform.
4. Repeat at lower rates and at the 33 MHz target, including A/B/A repetitions.
5. Exercise long idle, back-to-back reads/writes, malformed cycles, arming
   during a cycle, DMA boundaries, and capture-window exhaustion.

The acceptance comparison must use raw saved captures, not merely the expected
decoder text. Scope confirmation is required because the four-instruction loop
has very little maximum-frequency timing margin.

## Validation completed here

- Both Pico SDK targets compile with an ARM GCC toolchain.
- Software-generated two-phase captures decode all seven generic operations.
- Bad clock-phase pairs and malformed frames fail visibly.
- Long idle and adjacent cycles are covered by unit tests.
- Static PIO inspection confirms the sniffer has no pin-driving instruction.
- No QEMU source was changed.

Not completed: physical Pico execution, sustained 33 MHz testing, DMA-boundary
hardware testing, buffer-full hardware observation, or SDS814X edge/cycle
alignment. Until those are complete the design is **not ready for an M200
electrical-interface review**.
