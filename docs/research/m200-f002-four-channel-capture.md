# HW-EC-002 — Recovering F002 with four-channel equipment

Date: 2026-09-15  
Scope: design and research only. No probes were attached, no firmware was
run, and no QEMU or physical-machine state was changed.

## Executive conclusion

F002 can realistically be recovered without an SLA1016 in two ways:

1. **Immediately available, conditional evidence:** use the four-channel
   SDS814X HD for an overlapping three-pass reconstruction. Every pass records
   LCLK and LFRAME#, while adjacent passes share one LAD bit. Repeated
   A/B/C/C/B/A acquisitions can prove or disprove cycle-level correspondence.
   If the response changes between boots, this method must report ambiguity
   rather than splice unrelated nibbles.
2. **Better eventual evidence using inexpensive owned hardware:** an RP2040
   PIO/DMA sniffer can capture LFRAME# plus LAD[3:0] synchronously with LCLK.
   It is technically viable but remains experimental until its input-only
   startup and timing are bench-validated. Direct Pico attachment is not yet
   acceptable because RP2040 pads power up with pulls and their input
   capacitance is not specified sufficiently for this board-level load.

The recommended next physical experiment is the scope-only overlapping
multi-pass capture. It uses hardware already owned and does not require
building or trusting a new active sampler.

## 1. Actual LPC timing requirement

### Specification-level timing

The M306K5 documentation states:

- LPC operation is synchronized to LCLK, maximum 33 MHz;
- minimum LCLK period is 30 ns;
- minimum high and low widths are 11 ns;
- LAD setup to LCLK is 9 ns;
- input hold is 0 ns;
- output valid after LCLK is 2–11 ns;
- LPC I/O reads and writes use a basic 13-cycle sequence;
- each nibble is sampled at an LCLK rising edge;
- a falling LFRAME# begins a transaction.

The M200 design connects the ICH4-M LPC host to the M306K5 and LPC47N217.
The verified via map is:

| Signal | Via |
|---|---:|
| LCLK | 20 |
| LFRAME# | 15 |
| LAD0 | 10 |
| LAD1 | 12 |
| LAD2 | 13 |
| LAD3 | 14 |

The design guide labels the Super-I/O clock `CLK_SIOPCI`; the actual physical
frequency should be measured from captured LCLK rather than assumed. The
decoder must work up to the documented 33-MHz maximum.

### I/O write phases

For the M306K5's LPC interface:

| Rising-edge cycle | LAD value/owner | Meaning |
|---:|---|---|
| 1 | host `0000`, LFRAME# low | START |
| 2 | host `001X` | I/O write CYCTYPE/DIR |
| 3–6 | host | 16-bit address, high nibble first |
| 7–8 | host | data low nibble then high nibble |
| 9–10 | host/TAR | `1111`, then release |
| 11 | target | SYNC (`0000` for ready) |
| 12–13 | target/TAR | `1111`, then release |

XA2 latches address bit 2, distinguishing 62h data from 66h command/status.
IBF becomes set at completion of the write cycle, and clears only when the EC
CPU reads DBBIN.

### I/O read phases

| Rising-edge cycle | LAD value/owner | Meaning |
|---:|---|---|
| 1 | host `0000`, LFRAME# low | START |
| 2 | host `000X` | I/O read CYCTYPE/DIR |
| 3–6 | host | 16-bit address, high nibble first |
| 7–8 | host/TAR | `1111`, then release |
| 9 | target | SYNC |
| 10–11 | target | data/status low nibble then high nibble |
| 12–13 | target/TAR | `1111`, then release |

Reads of 66h return DBBSTS, including OBF bit 0 and IBF bit 1. Reads of 62h
return DBBOUT and clear OBF.

### Practical sampling rate

This experiment does not need analog characterization of 2-ns edges. It needs
the stable logical nibble associated with each rising LCLK edge. The practical
rates are:

| Rate | Samples per 30-ns worst-case LCLK | 50-Mpoint duration | Use |
|---:|---:|---:|---|
| 2 GSa/s | 60 | 25 ms | maximum timing detail; shortest window |
| 1 GSa/s | 30 | 50 ms | excellent |
| 500 MSa/s | 15 | 100 ms | recommended starting point |
| 250 MSa/s | 7.5 | 200 ms | good timing/window compromise |
| 100 MSa/s | 3 | 500 ms | marginal minimum; use only after validating edge phase |

At 500 MSa/s, 2-ns timestamps make the specified 9-ns setup and 2–11-ns
target-output region visible well enough to choose a stable sample point.
Decode by detecting LCLK rising edges from CH1 and sampling other channels
after their measured propagation settles, not by assuming a nominal period.

## 2. SDS814X HD clock and trigger capabilities

The SDS800X HD manual lists trigger sources as C1–C4, AC line, and D0–D15
when the optional digital pod exists. It does not list a separate EXT input or
external sampling-clock input. The SDS814X HD rear/front hardware therefore
does not provide a documented way to use LCLK as acquisition clock without
occupying CH1. Pattern triggering combines acquired channels; it does not add
an input. Serial trigger/decoder support is I2C, SPI, UART, CAN, and LIN, not
LPC.[^siglent-manual]

Sequence mode can retain up to 80,000 triggered segments and has a documented
minimum 2-us trigger interval. That dead interval is much longer than adjacent
LPC transactions can be, so triggering one segment per LFRAME# cannot prove a
complete transaction sequence. Sequence mode is useful for exploratory
waveforms, not the authoritative F002 capture.

Consequently, the proposed arrangement
`CH1..CH4=LAD[3:0]` cannot also obtain a hardware LCLK reference. The scope
cannot externally clock those four channels from LCLK.

## 3. Four-LAD-only capture

Capturing LAD0–LAD3 at 1–2 GSa/s can reveal 4-bit plateaus near 30-ns cadence.
Known START, CYCTYPE, 0062/0066 addresses, and request data form a distinctive
pattern. In favorable traces, LCLK phase can be estimated from correlated LAD
transitions and the known cycle grammar.

It is not independently reliable because:

- LAD is allowed to remain unchanged across adjacent clocked nibbles, so no
  edge identifies those sample boundaries;
- idle/turnaround levels do not uniquely replace LFRAME#;
- variable SYNC wait states alter cycle length;
- unrelated LPC traffic can contain the same short nibble pattern;
- target data becomes valid after the clock edge, while host data is valid
  before it, making an assumed phase particularly dangerous.

Classification: **POSSIBLE WITH VALIDATION**, not reliable. It is valuable as
a fourth cross-check after a framed multi-pass result, but it should not be
the sole evidence for response bytes.

## 4. Scientifically defensible multi-pass reconstruction

### Basic two-pass split

The obvious split is:

- A: LCLK, LFRAME#, LAD0, LAD1
- B: LCLK, LFRAME#, LAD2, LAD3

It preserves framing and edge timing in both runs. However, it has no shared
LAD bit. Identical LFRAME pulse timing and transaction length do not prove
identical address/data nibbles. A state-dependent response could be silently
combined into a byte that never existed.

Basic two-pass classification: **CONDITIONAL** and insufficient by itself for
a high-confidence test vector.

### Improved overlapping three-pass design

Use:

- A: CH1 LCLK, CH2 LFRAME#, CH3 LAD0, CH4 LAD1
- B: CH1 LCLK, CH2 LFRAME#, CH3 LAD1, CH4 LAD2
- C: CH1 LCLK, CH2 LFRAME#, CH3 LAD2, CH4 LAD3

This costs a third boot but gives cycle-by-cycle overlap:

- A.LAD1 must equal B.LAD1 for every aligned sample/cycle;
- B.LAD2 must equal C.LAD2 for every aligned sample/cycle.

Acquire in a palindrome such as A/B/C/C/B/A, then repeat the full set. This
tests drift and state evolution rather than assuming deterministic POST.

### Alignment procedure

1. Threshold each analog signal offline, retaining raw voltage and time.
2. Detect every LCLK rising edge and LFRAME# falling edge.
3. Represent each LPC transaction by:
   `ordinal, normalized start time, cycle count, LFRAME shape, observed LAD
   bitstrings, SYNC/wait length`.
4. Locate a long unique window containing candidate F002 traffic, not merely
   one 13-cycle frame.
5. Align transactions by ordinal within that unique window and LCLK cycles
   within each frame. Do not align by wall-clock time alone.
6. Require exact shared-bit agreement across A↔B and B↔C for the whole window,
   including request, status polls, and response reads.
7. Decode complete nibbles only after those checks pass.

### Strict acceptance criteria

A reconstructed result is accepted as Level 1B only if:

- all captures contain an identical ordered LFRAME transaction grammar from
  before `66h <- 9Eh` through the final response read;
- transaction counts and per-transaction LCLK/SYNC counts are identical;
- the known request decodes exactly as `66:9E, 62:11, 62:10, 66:9F`;
- shared LAD1 and LAD2 streams match cycle-for-cycle in all overlapping runs;
- at least three independent complete A/B/C sets produce the same response;
- bracketing palindrome runs show no sequence drift;
- no alignment insertion/deletion or “best fit” is needed;
- raw traces and the alignment manifest are preserved.

If a shared bit, frame count, SYNC count, response length, or inferred payload
differs, the result is **ambiguous**. It may indicate real state-dependent EC
behavior—which is valuable—but different boots must not be combined.

This approach is scientifically defensible under those conditions, but is
still weaker than simultaneous six-signal acquisition because identity across
boots is demonstrated indirectly.

## 5. Trigger without PJ2000

Use a long single acquisition and trigger on the first useful LFRAME# falling
edge after manual power-on:

- arm the scope in Normal/Single mode before applying laptop power;
- CH2 LFRAME# falling-edge trigger at approximately mid-rail;
- use 0–5% pre-trigger and the rest post-trigger;
- start at 500 MSa/s / 50 Mpoints (100 ms);
- if F002 is not present, repeat at 250 MSa/s (200 ms), retaining at least
  7.5 samples per worst-case LCLK period;
- use fixed memory/sample-rate mode so all runs share the same timebase.

The first LFRAME event may precede F002 substantially; that is why deep normal
acquisition is preferred over segmented mode. F002 is located offline by the
known request pattern assembled through overlap, not by a PJ trigger.

If firmware/initial bus activity begins before the selected trigger can arm,
use a circular acquisition with trigger position near the record start and
power the laptop only after the scope reports armed. Do not use AC-line trigger
or PJ2000.

## 6. RP2040/Pico passive-sniffer feasibility

### Electrical facts

RP2040 GPIO is a 3.3-V domain and is not 5-V tolerant. The LPC nets are
nominally 3.3 V, so no divider is required. But RP2040 GPIO reset defaults are
not ideal for direct attachment: output-enable resets clear (inputs), while
pad reset configuration includes weak pulls. Raspberry Pi documentation says
GPIOs revert to inputs on reset and applies default pull states; RP2040
references identify reset pull-downs around 50–80 kΩ.[^rp2040-gpio] Even a
weak pull violates the desired “no added pulls” contract until software
disables it.

The RP2040 datasheet does not provide a sufficiently useful maximum input
capacitance specification for this loading analysis. Direct flying-wire Pico
attachment is therefore **not yet approved**.

### Timing architecture

PIO does not simply replace its state-machine clock with LCLK. A state machine
running from the RP2040 system clock can instead:

1. wait for LFRAME# low;
2. wait for an LCLK rising transition;
3. sample contiguous `LFRAME#+LAD[3:0]` inputs with `IN PINS,5`;
4. wait for LCLK low;
5. repeat and pack samples into the RX FIFO;
6. use DMA to SRAM.

At the stock 125 MHz, one PIO instruction is 8 ns and the worst-case 30-ns
LCLK period provides only 3.75 PIO instruction slots. A naïve
`wait-high / in / wait-low` loop is close but plausible because WAIT stalls
without CPU intervention. Input synchronizers can add clock latency; bypassing
them improves phase but increases metastability exposure. Neither choice
should be trusted without bench timing validation against a synthetic
33-MHz source and known nibble pattern.

At the officially supported 133 MHz maximum, the instruction period is about
7.52 ns and yields about four instructions per minimum LCLK period. Do not
base the design on RP2040 overclocking.

### Buffering and storage

Do not continuously store six raw lines at 125 MHz: 264 KiB SRAM would cover
only a few milliseconds. Instead store one 5-bit sample per relevant LCLK edge
while inside a transaction, pack samples into 32-bit words, and attach a frame
marker/counter. A basic 13-cycle transfer is 65 data bits, about three words;
even thousands of LPC cycles then fit comfortably. DMA can drain the four- or
eight-word PIO RX FIFO without Linux scheduling.

This requires protocol-aware PIO and careful handling of additional SYNC wait
states. A circular pre-trigger buffer is unnecessary if it arms before the M200
and captures from first LFRAME through a bounded SRAM record, but the firmware
must expose overflow and never silently drop frames.

### Safe startup architecture

The safest Pico arrangement uses a **unidirectional high-input-impedance
3.3-V logic buffer** between motherboard and Pico:

- motherboard connects only to buffer inputs;
- buffer outputs connect only to Pico inputs;
- there is no electrical path from a Pico output to LPC;
- buffer input leakage/capacitance and 33-MHz timing must be specified;
- the buffer is powered and stable before laptop power-on;
- choose a part with defined power-off behavior/no back-powering;
- keep all leads short and use a solid nearby ground.

A 74LVC-family input buffer may satisfy these properties, but no exact part is
selected here because power-off behavior and input capacitance vary by suffix
and vendor. Six direct series resistors can limit accidental fault current but
do not remove Pico reset pulls or guarantee against output contention. Their
RC delay also consumes the 9-ns LPC setup margin. Therefore arbitrary resistor
values are not recommended without a measured load model.

Verdict: RP2040/Pico is **EXPERIMENTAL but viable**. After isolated bench
validation it can produce superior Level 1A simultaneous evidence at very low
cost. It is not the first physical M200 experiment.

## 7. Other compute platforms

### Raspberry Pi 5

Linux GPIO polling cannot deterministically sample six lines at 33 MHz.
RP1/DMA-based capture would require undocumented or substantially more complex
low-level work and still has GPIO pull/startup and electrical-loading issues.
CPU speed does not solve sampling determinism. It is inferior to RP2040 PIO.

### ESP32

Some ESP32 variants can use I2S/LCD-camera-style parallel input with DMA, but
capabilities, clock routing, GPIO matrix delay, DMA behavior, input thresholds,
and reset/strapping pulls vary by exact chip and board. No exact owned ESP32
variant is specified. It offers no evidence-based advantage over RP2040 and
has more boot-pin hazards. Classification: possible only after a separate
variant-specific design, not recommended here.

### Arduino

Classic AVR Arduino boards are generally 5-V, too slow, and lack sufficient
DMA/buffering. Faster 3.3-V Arduino-branded boards are variant-specific and
cannot be evaluated as one platform. Typical Arduino hardware is not realistic
for this capture.

### Inexpensive FPGA/CPLD

An FPGA with a fixed input-only pin assignment, dual-clock FIFO, and USB/UART
readout would be robust, but introduces new unowned hardware, HDL, I/O-bank
voltage configuration, and boot-state verification. It is a fallback after
scope reconstruction and Pico validation, not the current recommendation.

## 8. Offline decoder design

### Input forms

The decoder accepts either:

- simultaneous samples: timestamp, LCLK, LFRAME#, LAD0..3; or
- scope passes: timestamp, LCLK, LFRAME#, two named LAD bits plus run ID.

Always retain the original Siglent waveform files and export lossless CSV or
binary samples with channel scaling and time origin. Never retain only a
screenshot or manually transcribed byte list.

### Decode stages

1. Apply independently measured hysteretic thresholds to each analog channel.
2. Detect LCLK rising edges; sample LFRAME/LAD in the center of their measured
   stable region after accounting for channel deskew.
3. Split transactions at LFRAME# start.
4. Parse START, CYCTYPE/DIR, 16-bit address, TAR, all SYNC cycles, and data.
5. For multi-pass input, align frame signatures and require shared-bit equality
   before merging.
6. Emit TSV/JSON fields:
   `run, timestamp, frame, lclk_cycle, start, direction, address, data,
   sync, raw_bits, alignment_group`.
7. Filter exact I/O addresses 0066h and 0062h.
8. Find `W66=9E, W62=11, W62=10, W66=9F` and retain all following status and
   data reads until the response is complete or the BIOS retries.

Sigrok includes an LPC decoder requiring LFRAME#, LCLK, and LAD[3:0]. It can
be used after importing merged samples, but a small custom decoder is better
for reporting raw cycles, partial-bit signatures, overlap mismatches, and
multi-pass provenance. Sigrok is an independent cross-check, not the sole
analysis path.[^sigrok]

## 9. Exact physical result required

The capture must recover:

- the four known writes with correct port and command/data distinction;
- every 66h status read between and after them;
- whether the BIOS observes IBF asserted before clear;
- the first status read with OBF asserted;
- the first response byte interpreted by BIOS as length;
- exactly that many payload bytes from subsequent 62h reads;
- all timing and retry behavior through completion/failure.

No byte may be filled from the historical synthetic `01 05` or `01 01`.

## 10. Evidence levels for constrained capture

| Level | Requirement | Meaning |
|---|---|---|
| 1A | one true simultaneous six-signal passive capture from a validated custom sniffer | authentic physical test vector, not algorithm |
| 1B | ≥3 independent overlapping A/B/C scope sets satisfying every alignment criterion | reconstructed authentic test vector with explicit cross-boot limitation |
| 2 | repeated independent captures yield identical request/response and framing | strong deterministic-response evidence for unchanged machine state |
| 3 | firmware recovered or multiple differing vectors explain algorithm/state dependency | sufficient understanding for a general backend |

If multi-pass response data is not stable, it remains a set of partial
observations below Level 1B. A simultaneous Pico trace would then become the
next technical avenue, but that is not the experiment recommended in this
document.

## 11. Decision matrix

Scores: 5 is best, 1 is worst. “Owned” reflects the project-supplied hardware
list; exact ESP32/Arduino/Pico possession still needs confirmation.

| Approach | Safety | Timing | Simultaneous | Difficulty | Existing/low cost | Evidence | Overall |
|---|---:|---:|---:|---:|---:|---:|---|
| SDS four-LAD only | 5 | 3 | 4 of 6 | 3 | 5 | 2 | validation aid only |
| SDS overlapping three-pass | 5 | 5 per pass | 4 per boot | 3 | 5 | 4 if strict criteria pass | **best immediate** |
| RP2040 PIO + validated buffer | 4 | 4 pending bench test | all 6 | 2 | 5 | 5 after validation | best inexpensive Level 1A path |
| Raspberry Pi 5 | 3 | 1 | nominally all | 1 | 4 | 1 | reject |
| ESP32 parallel/DMA | 3 | 3, variant unknown | potentially all | 2 | 4 | 3 after validation | inferior to Pico here |
| typical Arduino | 1–3 | 1 | variant dependent | 2 | 4 | 1 | reject |
| inexpensive FPGA/CPLD | 4 after design | 5 | all 6 | 1 | 2 | 5 | later fallback |

## 12. Mandatory verdicts

**A. Can F002 realistically be recovered without SLA1016? — YES.** A strict
overlapping scope reconstruction can produce Level 1B evidence; a validated
RP2040 sniffer can later produce simultaneous Level 1A evidence.

**B. Best method using hardware already owned:** SDS814X HD overlapping
three-pass capture, A=`CLK/FRAME/LAD0/LAD1`,
B=`CLK/FRAME/LAD1/LAD2`, C=`CLK/FRAME/LAD2/LAD3`, repeated in palindrome
order and merged only after exact shared-bit/frame validation.

**C. Is RP2040/Pico a viable passive LPC sniffer? — EXPERIMENTAL.** PIO and
DMA are capable in principle, but external-clock phase, synchronizer latency,
overflow handling, reset pulls, and input loading require bench validation and
a unidirectional buffer before motherboard use.

**D. Can SDS814X alone recover the response? — POSSIBLY WITH MULTI-PASS.** It
cannot produce a conventional simultaneous six-wire trace.

**E. Is two-pass LAD reconstruction scientifically defensible? —
CONDITIONAL.** The non-overlapping two-pass split is weak. The recommended
overlapping three-pass refinement is defensible only if all strict acceptance
criteria pass across at least three complete sets.

**F. Minimum signals that must be captured simultaneously:** for each
defensible scope pass, LCLK + LFRAME# + two LAD bits. For unconditional
single-boot byte recovery, all six are required simultaneously.

**G. Recommended electrical interface:** the scope's normal 10×
high-impedance probes, shortest possible tips/ground springs at the verified
vias, one verified nearby board ground, laptop battery-only with every other
cable removed. No pulls, dividers, or series components are required for the
scope pass.

**H. Recommended sampling method/rate:** normal single-shot deep-memory
capture, 500 MSa/s and 50 Mpoints initially (100 ms); fall back to 250 MSa/s
for a 200-ms window. Decode from measured LCLK rising edges.

**I. Recommended trigger:** CH2 falling edge on verified LFRAME#, scope armed
before power-on, no PJ2000 dependency.

**J. Expected evidence level:** Level 1B after three independent A/B/C sets
all pass exact alignment and yield identical response; otherwise no recovered
byte claim.

**K. Exactly one recommended next physical experiment using hardware already
owned:** perform one non-invasive feasibility set of six cold boots in
palindrome order A/B/C/C/B/A with the SDS814X HD at 500 MSa/s, 50 Mpoints,
triggered on LFRAME# after the scope is armed. Save every raw waveform. Decode
only the known request and test shared-bit/frame alignment; do not yet claim a
response unless the full Level 1B criteria can subsequently be met.

## Sources

1. Mitsubishi, *M16C/6K7 Group Datasheet*, Rev. 1.0, LPC Bus Interface
   pp. 147–155 and electrical timing pp. 239–247. Local:
   `m200-data/M200DATASHEETS/M306K5F8LRP_M16C6K7.PDF`.
2. Toshiba, *Portege M200 Maintenance Manual (960-457)*. Local:
   `m200-data/toshiba_portege_m200.pdf`.
3. SMSC, *LPC47N217 Datasheet*. Local:
   `m200-data/M200DATASHEETS/LPC47N217.pdf`.

[^siglent-manual]: Siglent, *SDS800X HD Series User Manual*, trigger sources,
sequence mode, and trigger/decoder capabilities: https://int.siglent.com/u_file/download/24_02_20/SDS800X_HD_UserManual_EN01A.pdf
[^rp2040-gpio]: Raspberry Pi, *RP2040 Datasheet* and GPIO/PIO documentation: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
[^sigrok]: Sigrok LPC decoder: https://sigrok.org/wiki/Protocol_decoder%3ALpc
