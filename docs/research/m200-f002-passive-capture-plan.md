# HW-EC-001 — Passive capture plan for the authentic M200 F002 EC exchange

Date: 2026-09-15  
Scope: design only; no probes attached and no physical or QEMU action taken.

## Executive decision

The safest complete observation is a passive six-signal LPC acquisition at
the already continuity-verified vias, using the existing Siglent SDS814X HD
with its supported SLA1016 16-channel digital probe option. Capture
`LCLK`, `LFRAME#`, and `LAD[3:0]` together. If synchronization is needed, use
one analog channel of the same scope on the **independently re-verified
motherboard-output** PJ2000 UART net. Do not use a second grounded USB-TTL
adapter during the LPC acquisition.

This method observes the BIOS's naturally occurring transaction. It does not
write a host command, drive LPC, enter programming mode, or alter persistent
state. The four analog channels of the SDS814X HD alone cannot reliably decode
the six-wire LPC transaction.

## 1. Best observation point

### 1.1 LPC bus — recommended

The verified vias are preferable to IC pins:

| Via | Signal | LPC role |
|---:|---|---|
| via20 | `LCLK` | sampling clock, input to EC |
| via15 | `LFRAME#` | host cycle start/abort indication, input to EC |
| via10 | `LAD0` | multiplexed bidirectional nibble bit 0 |
| via12 | `LAD1` | multiplexed bidirectional nibble bit 1 |
| via13 | `LAD2` | multiplexed bidirectional nibble bit 2 |
| via14 | `LAD3` | multiplexed bidirectional nibble bit 3 |

These mappings were established by prior physical continuity testing to the
LPC47N217. Because LPC is a shared bus, the same cycles destined for the
M306K5 are visible there. The vias avoid soldering to fine-pitch M306K5 or
LPC47N217 leads and are the first-choice probe points.

The M306K5 documentation names the corresponding MCU functions
`P30/LAD0`, `P31/LAD1`, `P32/LAD2`, `P33/LAD3`, `P34/LFRAME`, and
`P36/LCLK`. It defines LAD as bidirectional; LFRAME and LCLK are inputs. The
documented LPC operating clock is synchronized to LCLK, maximum 33 MHz, with
a minimum 30 ns period.

### 1.2 DBBIN/DBBOUT — not an alternate probe bus

`DBBIN0..3` and `DBBOUT0..3` are internal SFR-backed latches, not a documented
external byte-wide interface. A host write latches the LPC data into DBBIN;
an EC-CPU read consumes it and clears IBF. EC firmware writes the same SFR
address to populate DBBOUT; a host read returns it and clears OBF.

The MCU has selectable OBF status outputs, but the available M200 evidence
does not prove that a useful OBF pin is routed to a safe test point. Such a
pin would reveal only status timing, not the response byte or I/O address.
It is therefore not a substitute for LPC capture and must not be probed on
speculation.

### 1.3 Toshiba debug facilities

PJ2000 provides POST checkpoint timing, not EC mailbox contents. It can mark
F002, narrowing the acquisition window, but cannot reveal the response.
There is no proven Toshiba debug/test connector exposing DBBIN or DBBOUT.

There is a safety-relevant PJ2000 documentation conflict:

- task-supplied mapping: pin 1 `DBGRX-B3P`, pin 2 `DBGTX-B3P`, pin 3 GND,
  pin 4 B3V;
- maintenance-manual table C-9: pin 1 B3V, pin 2 `DBGRX-B3P` (output),
  pin 3 `DBGTX-B3P` (input), pin 4 GND.

This may be a connector-view/orientation or transcription difference. No
PJ2000 connection is permitted on names or pin numbers alone. Ground, B3V,
and the motherboard-output UART net must first be re-identified against the
board connector orientation and electrically verified. Only the
motherboard-output signal may be observed.

## 2. Software-only passive observation

No software-only passive path is established.

- The transaction occurs during early BIOS POST, before a DOS or operating
  environment can log port traffic.
- After the BIOS reads DBBOUT, hardware clears OBF. The M306K5 contract does
  not promise a later readable transaction history or preserved response.
- A DOS program that sends 9E/9F would create a new transaction and is
  forbidden, not passive.
- Reading port 62 later cannot recover a byte already consumed by BIOS.
- BIOS instrumentation requires modifying execution or firmware.
- The Banias-era machine offers no applicable pre-boot hardware
  virtualization trace facility for transparent port-I/O interception.
- No existing HCI/SCI, SMM, PJ2000, or diagnostic-log service has been proven
  to expose historical EC mailbox bytes.

Therefore electrical observation of the naturally occurring LPC exchange is
required for a complete passive test vector.

## 3. LPC cycles that must be decoded

The M306K5 LPC section specifies 13 basic LCLK cycles for its I/O reads and
writes. Values are sampled at rising LCLK edges and communication begins at a
falling LFRAME edge.

### Host I/O write

| Cycle(s) | LAD nibble/owner | Meaning |
|---:|---|---|
| 1 | host `0000` while LFRAME# low | START |
| 2 | host `001X` | I/O write CYCTYPE/DIR |
| 3–6 | host | 16-bit address, nibbles `[15:12]`, `[11:8]`, `[7:4]`, `[3:0]` |
| 7–8 | host | data `[3:0]`, then `[7:4]` |
| 9 | host `1111` | TAR |
| 10 | host releases LAD | turnaround |
| 11 | target `0000` | SYNC ready/OK |
| 12 | target `1111` | TAR; XA2 and IBF become set; IBF interrupt generated |
| 13 | target releases LAD | bus returns Hi-Z |

Address bit A2 is latched into XA2. Thus writes to 62h have XA2=0/data
semantics and writes to 66h have XA2=1/command semantics.

### Host I/O read

| Cycle(s) | LAD nibble/owner | Meaning |
|---:|---|---|
| 1 | host `0000` while LFRAME# low | START |
| 2 | host `000X` | I/O read CYCTYPE/DIR |
| 3–6 | host | 16-bit address, high nibble first |
| 7 | host `1111` | TAR |
| 8 | host releases LAD | turnaround |
| 9 | target `0000` | SYNC ready/OK |
| 10–11 | target | data/status `[3:0]`, then `[7:4]` |
| 12 | target `1111` | TAR; a DBBOUT read clears OBF and requests OBE interrupt |
| 13 | target releases LAD | bus returns Hi-Z |

Reads of 66h select DBBSTS because A2=1. The returned status includes OBF bit
0 and IBF bit 1. Reads of 62h select DBBOUT because A2=0.

### Minimum signal set

All six signals are required for a reliable general decode:

`LCLK`, `LFRAME#`, `LAD0`, `LAD1`, `LAD2`, `LAD3`.

All four LAD bits are necessary because START, cycle type/direction, every
address nibble, SYNC/TAR, and both data nibbles are encoded across the full
four-bit bus. LFRAME# provides unambiguous cycle boundaries. LCLK supplies the
specified sampling edge. Omitting any one requires assumptions or
cross-boot waveform reconstruction and cannot yield a reliable response byte.
LRESET#, SERIRQ, CLKRUN#, and LDRQ# are not needed for ordinary 62h/66h I/O
cycle decoding.

## 4. Electrical requirements and risks

### Voltage domain

The M306K5 recommended operating range is VCC 3.0–3.6 V, nominal 3.3 V. For
LAD, LFRAME, and LCLK it documents VIH ≥ 0.6 VCC and VIL ≤ 0.2 VCC. The
LPC47N217 documentation independently gives 3.3-V operation and a 3.6-V
maximum input for LAD/LFRAME-class pins. The M306K5 timing table gives 9 ns
LAD setup, up to 11 ns clock-to-output, and 28 ns maximum release-to-float.

The capture input must therefore be 3.3-V compatible, present no active
pull-up/down, and use a threshold near 1.65 V. A 5-V-only logic threshold is
not acceptable. No capture input may ever be configured as an output.

### Recommended probe loading

The SLA1016 is specified at 100 kΩ in parallel with 8 pF, ±10-V dynamic
range, 120-Mbps maximum data rate, 8.3-ns minimum pulse width, and selectable
3.3-V LVCMOS threshold. It is supported by the SDS800X HD family.[^siglent]
This is suitable for 33-MHz LPC observation if leads are short. It is not
electrically invisible: six 8-pF probes add local capacitive load, so use the
verified vias, the shortest practical flying leads, one nearby ground, and no
clips hanging from fine-pitch pins.

Optional small series resistors are not prescribed here because their value,
lead capacitance, and placement would need signal-integrity verification and
could consume the M306K5's 9-ns setup margin. The commercial pod's specified
high-impedance inputs should be used as designed rather than adding an
uncharacterized network.

### Grounding

For the future experiment:

1. power the M200 from its battery, with AC adapter, docking station, USB,
   Ethernet, VGA, audio, and every other grounded cable disconnected;
2. use one measuring instrument and one short ground connection to a verified
   motherboard ground point;
3. confirm scope protective-earth behavior and zero hazardous potential
   between probe ground and board ground before connection;
4. never connect a ground clip to B3V or an unverified PJ2000 pin;
5. secure probes before power-up so no tip can bridge adjacent vias.

“Battery powered” reduces ground-loop risk but does not replace ground/net
verification: the mains-powered scope ground is normally protective earth.

## 5. Capture hardware assessment

| Equipment | Timing capability | Electrical/control concern | Verdict |
|---|---|---|---|
| SDS814X HD, four analog channels only | 100 MHz, 2 GSa/s, deep analog memory | only four simultaneous signals | useful for voltage/preflight or partial waveform; **cannot fully decode LPC alone** |
| SDS814X HD + SLA1016 | up to 16 digital inputs; official SDS800X HD support | 100 kΩ ∥ 8 pF per input; threshold must be set correctly | **recommended complete capture** |
| Raspberry Pi 5 GPIO/software | no guaranteed sustained synchronous 33-MHz six-line capture path established | GPIO initialization, pulls, OS jitter, accidental output risk | reject |
| Raspberry Pi Pico/RP2040 | PIO could theoretically perform deterministic input sampling and DMA[^rp2040] | custom firmware, finite buffer, 3.3-V-only GPIO, board-level pulls/boot configuration and loading need validation | technically plausible future custom sniffer, not safest first capture |
| ESP32 | no proven configuration here meeting synchronized LPC capture and passive-start guarantees | variant-dependent GPIO voltage, pulls, DMA and boot-pin behavior | reject without a separately validated design |
| Arduino | typical boards lack the rate/channel/buffer combination; some are 5-V systems | electrical incompatibility and insufficient deterministic capture | reject |

The SDS800X HD officially supports the SLA1016 option, so it extends the
known instrument rather than introducing an unverified MCU capture path.[^sds]

## 6. Acquisition configuration

### Channels

- D0: via20 / LCLK
- D1: via15 / LFRAME#
- D2: via10 / LAD0
- D3: via12 / LAD1
- D4: via13 / LAD2
- D5: via14 / LAD3
- optional analog CH1: separately verified PJ2000 motherboard-output UART

Set the digital threshold to 3.3-V LVCMOS (nominal 1.65 V). Disable any
software-configurable pull or output function; the SLA1016 inputs are used
only in their normal acquisition mode.

### Rate and depth

- preferred digital sample rate: **500 MSa/s** if available at the selected
  depth (about 15 samples per 33-MHz LCLK period);
- acceptable preferred compromise: **250 MSa/s** (about 7.5 samples/cycle);
- minimum for this capture: **100 MSa/s** (about 3 samples/cycle), only after
  verifying clean edge placement against LCLK;
- target capture window: at least **20 ms**, preferably **100 ms**, spanning
  the F002 marker through all response reads;
- corresponding raw depth: 2 M samples/channel at 100 MSa/s for 20 ms, or
  10 M samples/channel for 100 ms. At 250 MSa/s those become 5 M and 25 M.

Use the maximum supported digital memory and export the raw digital waveform,
not only screenshots. If the installed scope/pod combination cannot retain
at least 20 ms at 100 MSa/s, do not compensate by reconstructing LAD bits
from different cold boots. Instead the trigger must be moved closer to the
9E cycle or a deeper capture system obtained.

### Trigger and synchronization

Preferred trigger:

1. observe the known PJ2000 motherboard-output serial line with analog CH1;
2. configure 57,600 8N1 UART decode and trigger on the checkpoint bytes that
   encode F002, after first confirming the actual byte framing from an
   unambiguous passive trace;
3. retain roughly 20% pre-trigger and 80% post-trigger LPC data.

PJ2000 is used only as a temporal marker. Because the analog and digital
channels belong to the same SDS814X acquisition, no USB-host timestamp
correlation is required. If reliable UART pattern trigger is unavailable,
capture from the nearest reproducible PJ transition and locate F002 and
`66h <- 9Eh` offline. Do not connect a separate USB-TTL receiver during the
primary capture.

The fallback is an LPC digital-pattern trigger near a LFRAME# start followed
by the 0066h/9Eh write, but that normally needs a protocol-aware trigger and
is less straightforward than PJ2000 synchronization.

## 7. Decode and interpretation

Export the six digital channels to a format retaining sample timing. Sigrok
already provides an LPC decoder whose required probes are exactly LFRAME#,
LCLK, and LAD[3:0].[^sigrok] The raw waveform remains authoritative: verify
the decoder's nibble order and I/O-cycle handling against the M306K5
13-cycle description before accepting bytes.

Filter decoded I/O cycles to addresses 0062h and 0066h. A successful trace
must show, in order:

1. I/O write 0066h = 9Eh;
2. intervening I/O reads 0066h showing the BIOS's IBF polling, including any
   observed IBF=1 and the first later IBF=0;
3. I/O write 0062h = 11h and its status polling;
4. I/O write 0062h = 10h and its status polling;
5. I/O write 0066h = 9Fh and its status polling;
6. reads of 0066h until OBF bit 0 is set;
7. each subsequent I/O read 0062h, preserving exact order and timing;
8. status reads surrounding each response byte.

Interpret the first DBBOUT byte according to BIOS behavior as the response
length, then collect exactly that many payload bytes. Do not assume two bytes
total, `01 05`, or any specific payload. Retain the entire surrounding trace
so repeated OBF assertions and multi-byte framing are not lost.

For each host byte record:

- end of its LPC write cycle;
- first following 66h status read;
- first status read with IBF=0;
- number and spacing of status reads.

For each EC output byte record:

- first status read where OBF=1;
- following 62h read timestamp and byte;
- first subsequent status value;
- response-byte spacing.

These are host-visible service observations, not direct measurements of EC
firmware internal semantic completion.

## 8. Reproducibility protocol

Without deliberately changing any state, perform at least five ordinary cold
boots of the same M200 using the identical hardware setup. “Cold boot” here
means the project's normal repeatable power-off/power-on procedure; do not
remove batteries, clear CMOS, alter DMI, change firmware, or invoke EC
commands.

For every trace compare:

- exact request bytes and port classification;
- response length and every payload byte;
- IBF and OBF polling sequences;
- response latency relative to the 9F write;
- whether the first boot after probe installation differs from later boots.

Record ambient machine state non-invasively (AC disconnected as required for
measurement safety, battery present, machine previously off) but do not vary
it experimentally. A second physical M200 would later distinguish
machine-specific data from model-wide behavior, but is not needed for the
first test-vector result.

## 9. QEMU evidence levels

| Level | Physical evidence | Permitted conclusion/use |
|---|---|---|
| 1 | one complete authentic `9E 11 10 9F` request/response capture | one valid physical test vector; may support a diagnostic replay test, **not an algorithm or general backend** |
| 2 | repeated identical captures under unchanged conditions | strong evidence that this response is deterministic for that machine/state; still not proof of model-wide algorithm |
| 3 | firmware path recovered or multiple deliberately/non-deliberately differing authentic vectors explain state/input dependence | sufficient basis for an architectural Toshiba backend implementing the understood algorithm/state contract |

POST-001A must continue to model mailbox ownership independently of whatever
response is observed. Captured bytes must not be presented as M306K5 silicon
reset behavior.

## 10. Mandatory verdicts

**A. Safest observation method:** six-channel passive LPC capture at the
continuity-verified vias with SDS814X HD + SLA1016.

**B. Can it be completely passive:** **YES**, electrically, provided every
channel remains high-impedance input-only, no pulls are enabled, and the
single ground/net identities are re-verified. It still adds finite probe
capacitance, which is why short leads and repeat validation are required.

**C. Can the SDS814X HD alone decode the required transaction:** **NO.** Four
analog channels cannot simultaneously acquire LCLK, LFRAME#, and LAD[3:0].
Cross-boot reconstruction is not reliable.

**D. Best existing hardware for full capture:** the existing SDS814X HD
expanded with its supported **SLA1016** 16-channel logic probe. Without that
pod or an equivalent validated ≥6-channel analyzer, none of the listed
currently known devices is established as a safe complete recorder.

**E. Exact signals required:** `LCLK`, `LFRAME#`, `LAD0`, `LAD1`, `LAD2`,
and `LAD3`. PJ2000 motherboard TX is optional synchronization only.

**F. Recommended trigger/synchronization:** same-instrument analog UART
trigger on the independently re-verified PJ2000 F002 output, with 20%
pre-trigger and enough post-trigger memory to cover at least 20 ms,
preferably 100 ms.

**G. Electrical risks:** reversed PJ2000 numbering, grounding B3V, scope-earth
ground loops, probe slips/bridges, excessive lead capacitance, incorrect
threshold, hidden pulls, or accidental MCU GPIO output. Mitigate with
battery-only laptop operation, one verified short ground, verified nets,
secured probes, 3.3-V threshold, and commercial input-only pod configuration.

**H. Successful F002 capture:** one contiguous raw trace proving all four
host writes, intervening IBF polling, OBF assertion, and every 62h response
read sufficient to recover the length and full payload.

**I. Evidence level:** the first complete capture is **LEVEL 1**, a physical
test vector only. Five identical cold-boot captures raise it to LEVEL 2.

**J. Exactly one recommended next physical experiment:** with the M200
battery-powered and all external cables removed, use one SDS814X HD fitted
with SLA1016 to passively capture the six verified LPC vias during one natural
cold boot, using a separately re-verified PJ2000 motherboard-output signal on
analog CH1 solely to trigger near F002. No other instrument or connection is
used, and no signal is driven.

## Sources

1. Mitsubishi, *M16C/6K7 Group Datasheet*, Rev. 1.0, LPC Bus Interface
   pp. 147–155, LPC pins/function table p. 159, operation clock p. 163,
   electrical characteristics pp. 239–247. Local:
   `m200-data/M200DATASHEETS/M306K5F8LRP_M16C6K7.PDF`.
2. SMSC, *LPC47N217 Datasheet*, LPC pin/function and electrical tables.
   Local: `m200-data/M200DATASHEETS/LPC47N217.pdf`.
3. Toshiba, *Portege M200 Maintenance Manual (960-457)*, board topology and
   PJ2000 table C-9. Local: `m200-data/toshiba_portege_m200.pdf`.

[^siglent]: Siglent logic-probe specifications, SLA1016: https://int.siglent.com/u_file/download/25_11_17/Probe_DataSheet_EN02B_20250905_release.pdf
[^sds]: Siglent SDS800X HD product page, SLA1016 support: https://www.siglent.com/na/products-overview/sds800x-hd/
[^rp2040]: Raspberry Pi, RP2040 datasheet and PIO architecture: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
[^sigrok]: Sigrok LPC protocol decoder probe requirements: https://sigrok.org/wiki/Protocol_decoder%3ALpc
