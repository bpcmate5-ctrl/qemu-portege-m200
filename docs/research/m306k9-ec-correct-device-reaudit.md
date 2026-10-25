# EC-RE-002 — M306K9FCLRP correct-device re-audit

Date: 2026-09-15  
Scope: source-driven research only; QEMU and physical hardware were not changed.

## 1. Executive summary

The motherboard marking **M306K9FCLRP** is physical primary evidence. The
applicable silicon source is Renesas, *M16C/6K9 Group — Single-Chip 16-Bit CMOS
Microcomputer*, document `REJ03B0041-0100Z`, Rev. 1.00, 2003-06-06 (hereafter
“K9 datasheet”).[^k9]

The correction is material. M306K9FCLRP is a 144-pin M16C/60 device with 5 KiB
RAM and 128 KiB NEW DINOR user flash at E0000h–FFFFFh. It has four LPC host
channels and three I²C channels. The earlier K5 identification, 3 KiB RAM,
64-KiB flash layout, package-specific programming supply, and K5 pin claims are
**DISPROVEN** for the physical M200.

The central mailbox architecture survives independent K9 review. K9 directly
documents DBBIN, DBBOUT, DBBSTS, OBF bit 0, IBF bit 1, XA2 bit 3, user bits
2/4/5/6/7, input ownership, and output ownership. It also proves that a
programmable LPC channel can ignore address A2 and latch A2 into XA2; therefore
62h and 66h form a data/status pair when that channel is programmed to the
appropriate base. Channel 0 is instead fixed at 60h/64h.

POST-001A remains a useful one-channel board abstraction, not a full K9 LPC
block. Its principal unsupported policy is the M200 machine's initial
`user-status-reset=0x70`. K9 specifies DBBSTS reset as 00h. Authentic BIOS code
does require `(DBBSTS & 70h) == 70h` on the F002 receive path, so 70h has
firmware-contract support, but it must not be described as silicon reset state.
The code also refuses an EC-side DBBOUT write while OBF is already one. K9
describes DBBOUT as a writable latch whose write sets OBF, but does not specify
this already-full edge case. Both rejection and overwrite are therefore
unsupported choices; the architectural result is **UNKNOWN**, not permission
to replace one guess with another.

No authentic Toshiba 9Eh/9Fh algorithm, firmware image, ordinary host-command
ROM reader, or INT15/SMM firmware-dump primitive has been found. Natural F002
failure therefore remains correct.

## 2. Exact M306K9FCLRP identity

| Property | Exact-device result | Evidence and classification |
|---|---|---|
| Part | M306K9FCLRP, production flash version | K9 p.3 comparison and p.8 Table AA-2 — **PROVEN** |
| Family/core | M16C/6K9, M16C/60 Series 16-bit CPU core | description p.1; Fig. AA-3 and Table AA-1 pp.6–7 — **PROVEN** |
| Package | 144PFB-A, plastic 144-pin 16×16-mm TQFP/QFP, 0.40-mm pitch | Table AA-2 p.8; package drawing p.290 — **PROVEN** |
| Address space | 1 MiB architectural address space, 00000h–FFFFFh | memory map Fig. AA-6 p.13 and interrupt-vector addresses — **PROVEN** |
| RAM | 5 KiB | Table AA-2 p.8 — **PROVEN** |
| User flash | 128 KiB NEW DINOR | pp.3, 8; Table AB-1/Fig. AB-1 pp.259–260 — **PROVEN** |
| User-ROM range | E0000h–FFFFFh inclusive | p.3 comparison; Fig. AB-1 p.260 — **PROVEN** |
| Boot ROM | Separate 4-KiB boot-ROM address space mapped FF000h–FFFFFh in boot/flash access context; factory serial control program; parallel-mode rewrite only | Table AB-1 and Fig. AB-1 pp.259–260; pp.261, 273, 276 — **PROVEN** |
| Normal VCC | 3.0–3.6 V | pp.1, 7, 10 — **PROVEN** |
| Flash program supply | FVCC, 3.0–3.6 V for programming | p.3; pin description p.10; Table EE-1 p.275 — **PROVEN** |
| CPU clock | XIN/XOUT main clock; minimum instruction time 62.5 ns at 16 MHz/0 wait; reset starts divided-by-8 BCLK | pp.1, 7; clock section pp.30–38 — **PROVEN silicon capability**, Toshiba board rate **UNKNOWN** |
| LPC | Four channels | p.1; Table AA-1 p.7; LPC section p.149 — **PROVEN** |
| I²C | Three channels on FCLRP | p.1; Table AA-2 p.8; I²C section — **PROVEN** |
| Other serial | UART0–2 plus synchronous SIO3/4; three PS/2 channels | Table AA-1 p.7 — **PROVEN** |
| Flash modes | parallel I/O, standard serial I/O, CPU reprogram | Table AB-1 p.259 — **PROVEN** |
| Mode/reset | normal: M0=L; boot/standard serial: reset release with M0=H, M1=L; RESET active low | pp.10, 261, 275–276 — **PROVEN** |

The boot ROM is not an additional 4 KiB appended to the 128-KiB user range.
It is a distinct flash access context sharing the top architectural addresses.
Conflating the two would be erroneous.

## 3. Memory map and firmware storage

Fig. AB-1 (p.260) divides M306K9FCL user flash as follows:

| User range | Block | Size |
|---|---:|---:|
| E0000h–EFFFFh | 6 | 64 KiB |
| F0000h–F7FFFh | 5 | 32 KiB |
| F8000h–F9FFFh | 4 | 8 KiB |
| FA000h–FBFFFh | 3 | 8 KiB |
| FC000h–FDFFFh | 2 | 8 KiB |
| FE000h–FEFFFh | 1 | 4 KiB |
| FF000h–FFFFFh | 0 | 4 KiB |

The complete user range is exactly 0x20000 bytes. The reset vector is at
FFFFCh–FFFFFh; the seven ID bytes occupy the final byte of seven exception
vectors (p.272 Fig. BB-7; p.284 Fig. EE-11).

**STRONG INFERENCE:** Toshiba runtime EC firmware resides principally in this
internal user flash. The part integrates the CPU and user flash, and no M200
primary evidence identifies an external program ROM. External EEPROM could
hold configuration, but an external program store is **UNKNOWN**, not proven.

Local package/media searches previously found no credible K9 image. Rechecking
the authentic 336,128-byte BIOS payload, 512-KiB physical FWH dump, M200 BIOS
update files, service floppy, and candidate 128-KiB-scale regions produced no
blob with a validated K9 vector map, reset target, M16C control flow, and update
wrapper. `V4S_004A/0055` remain Wacom LC87F1564 firmware; Satellite 1800
`EC30B100.BIN` targets different K7-era hardware. Verdict: a local authentic
M306K9 image is **UNKNOWN/not found**, not proven absent from every opaque BIOS
module.

## 4. K9 LPC host interface

Primary reference: K9 “LPC Bus Interface,” pp.149–166; Fig. GF-1 through
Fig. GF-8; Fig. GF-4 “Data bus buffer status register,” p.154; Table GF-3,
p.163.

### 4.1 Channels and decode

- Four single-byte data-buffer channels exist (**PROVEN**).
- Channel 0 is fixed at 0060h/0064h (**PROVEN**, p.149 and p.152).
- Channels 1–3 have programmable 16-bit `LPCiADH/L` registers at
  02D0h–02D5h (**PROVEN**, pp.152, 155–156).
- For channels 1–3, address bit 2 is not decoded. Host A2 is latched as XA2 on
  writes; reading the address register returns its A2 bit as zero
  (**PROVEN**, pp.149, 152).
- `LPCCON` bit 3 enables LPC pin function and bits 4–7 enable channels 0–3
  (**PROVEN**, pp.149, 157).

Thus an enabled programmable channel configured for 0062h responds at both
0062h and 0066h. At 0062h A2=0, and at 0066h A2=1. The actual M200 channel
number and Toshiba EC-side programming are **UNKNOWN** because no EC firmware
has been recovered. It cannot be channel 0's intrinsic 60h/64h pair.

### 4.2 Latches and status

| Item | K9 behavior | Verdict on old assumption |
|---|---|---|
| DBBIN0–3 | Host I/O write latches byte; EC reads it from SFR 02C0/2/4/6h | **PROVEN, retained** |
| IBF bit 1 | Set when host writes DBBIN; cleared simultaneously when EC CPU reads DBBIN | **PROVEN, retained** |
| DBBOUT0–3 | EC write to same SFR transfers byte to output latch | **PROVEN, retained** |
| OBF bit 0 | EC write sets; host data-buffer read clears | **PROVEN, retained** |
| XA2 bit 3 | Host write latches address A2 | **PROVEN, retained** |
| U2/U4–U7 | EC/slave software-readable/writable user flags | **PROVEN, retained** |
| status read | Host reads DBBSTS when A2=1 | **PROVEN, retained** |
| data read | Host reads DBBOUT when A2=0; clears OBF | **PROVEN, retained** |
| input interrupt | Rising IBF generates channel IBF interrupt request | **PROVEN but unmodeled** |
| output-empty interrupt | Host data read clearing OBF generates OBE request, selectable against IBF1–3 | **PROVEN but unmodeled** |

K9 distinguishes electrical completion from firmware consumption. The LPC
write returns SYNC-ready during the bus cycle; XA2/IBF become set at the 12th
cycle, and IBF remains set until an EC CPU read (pp.158–160, Fig. GF-7).
Likewise semantic Toshiba command completion is not implied by IBF clearing.

### 4.3 Full-latch edge cases

DBBIN is one latch per channel. The manual says each host write latches input
data and sets IBF; it documents no WAIT/error interlock conditioned on existing
IBF. Therefore replacement of DBBIN/XA2 by a second nonconforming host write is
a **STRONG INFERENCE**, while the exact interrupt behavior without an IBF
rising edge is **UNKNOWN**. POST-001A's overwrite behavior is reasonable, but
its comment claiming explicit documentation of the overrun should be softened.

For DBBOUT, writing the SFR transfers data to DBBOUT and sets OBF. The K9 text
does not define behavior when OBF is already one. POST-001A returns false and
preserves the old byte, but that rejection is **UNSUPPORTED**. Overwrite is a
plausible one-latch interpretation, not proven behavior. No queue is
documented; defer changing this edge case pending stronger evidence.

## 5. Reset semantics

Fig. GF-4 p.154 gives DBBSTS0–3 reset value `00000000b`: OBF=0, IBF=0, XA2=0,
and all U bits zero. DBBCON0/1, LPC1–3 address registers, and LPCCON also reset
to zero (pp.153, 155–157). Table GF-3 p.163 says LRESET initializes XA2, IBF,
and OBF to zero; it explicitly calls out U-bit initialization to zero only for
DBBSTS0 under LRESET. Input data becomes unstable and slave writes to DBBOUT
are inhibited while LRESET is low.

This yields three distinct states:

1. **Silicon/system reset:** DBBSTS=00h, channels and LPC pin function disabled.
2. **Toshiba EC firmware initialization:** firmware enables/configures a
   programmable channel and may set U flags.
3. **QEMU board policy:** the machine currently instantiates the already-live
   62h/66h endpoint with U bits 6:4 equal to 111 (`0x70`).

Authentic BIOS at file offsets 7C1E5h–7C1EAh and 7C209h–7C211h masks returned
status with 70h and requires equality with 70h. That is **PROVEN BIOS contract**
and a **STRONG INFERENCE** that real Toshiba EC firmware sets U6:U4 by this
stage. It does not prove power-on/reset state, when the flags are initialized,
or that all Toshiba commands use them identically. Thus 0x70 is definitely not
silicon reset; the current reset property is an architectural shortcut that
should ultimately move into an evidence-backed Toshiba backend initialization
stage. This research pass makes no code change.

QEMU also conflates full device reset with LRESET nuances. That is acceptable
for current POST abstraction but **CORRECT BUT INCOMPLETE** for a reusable K9
device.

## 6. POST-001A code audit

Current architectural HEAD is `de8a7ca12e90fdf0803cdd28874ef23a044f3020`,
not the task's stale expected `cee5b674...`; POST-001A is already committed in
HEAD. Existing unrelated diagnostic changes remain unstaged.

| Code behavior | K9 source | Status | Required future change |
|---|---|---|---|
| Separate one-byte DBBIN/DBBOUT | pp.152, 154 | CORRECT | Rename K5 comments/types to K9/generic K9 channel |
| OBF=bit0, IBF=bit1, XA2=bit3 | Fig. GF-4 p.154 | CORRECT | None |
| U mask `0xf4` | Fig. GF-4 p.154 | CORRECT | None |
| host write sets DBBIN/IBF | pp.152, 158–160 | CORRECT | None |
| backend consume clears IBF | p.152, p.159 | CORRECT | None |
| output byte sets DBBOUT/OBF | p.152 | CORRECT normally; full-latch case UNKNOWN | Do not guess OBF-already-set behavior |
| host 62h read returns DBBOUT and clears OBF | pp.152, 159, 161 | CORRECT | None |
| host 66h read returns status without clearing OBF | pp.152, Fig. GF-8 | CORRECT | None |
| 62h/66h represented as A2 pair | pp.149, 152 | CORRECT for channel 1–3 | Document configurable channel; not channel 0 |
| always-enabled fixed endpoint | LPCCON/address regs pp.155–157 | CORRECT BUT INCOMPLETE board abstraction | Add full config only if EC firmware/use requires it |
| no IBF/OBE IRQ model | Fig. GF-1 pp.150–151 | CORRECT BUT INCOMPLETE | Backend notification models service opportunity; add IRQ only with EC-core model |
| BH-delayed consume | no silicon time bound | QEMU implementation mechanism | Retain no fabricated delay; do not claim physical latency |
| second host write overwrites DBBIN | one-latch description p.152 | STRONG INFERENCE | Soften “documented” comment; preserve until contrary evidence |
| DBBOUT write rejected if OBF=1 | p.152 does not specify full-latch behavior | UNSUPPORTED; alternative UNKNOWN | Defer change pending evidence |
| reset DBBIN/DBBOUT bytes to zero | Table GF-3 says input unstable, output unwritable under LRESET | UNSUPPORTED value but unobservable when flags clear | Treat latch contents unspecified; flags determine visibility |
| reset flags zero | Fig. GF-4 p.154; Table GF-3 p.163 | CORRECT silicon default |
| M200 reset U flags 0x70 | BIOS requires `(status & 70h)==70h` during F002, but K9 resets DBBSTS to 00h | TOSHIBA POLICY, NOT SILICON; placement at reset is UNSUPPORTED | Move to future backend initialization; physical capture can establish timing |
| VMState migrates latches/flags and post-load schedules pending input | QEMU correctness | CORRECT | Migration should also preserve backend state once one exists |
| reset cancels BH; unrealize deletes BH | QEMU lifecycle | CORRECT | None |

The default backend consumes unsupported input and emits nothing. This models
an unspecified running EC firmware choosing to consume the byte, not raw
reset-state silicon. That distinction should be documented. It correctly does
not fabricate `01 05` or `01 01`.

## 7. Toshiba firmware versus silicon

| Layer | Established behavior | Evidence |
|---|---|---|
| K9 silicon | address decode, latches, DBBSTS, IBF/OBF/XA2, channel interrupts | K9 pp.149–166 — **PROVEN** |
| Toshiba EC firmware | consumes BIOS bytes and ultimately emits a length/payload | physical board existence plus BIOS polling — **STRONG INFERENCE**; exact handler unavailable |
| Toshiba framing | CMD 9E, DATA bytes, CMD 9F, then length/payload | authentic BIOS disassembly — **PROVEN** |
| Security/model algorithm | maps request to authentic response | **UNKNOWN** |

Neither K9 nor LPC defines 90h, 91h, 9Eh, or 9Fh. Those are Toshiba firmware
semantics.

## 8. Standard serial programming interface

Primary reference: flash section pp.259–289, especially Table EE-1 p.275,
“Standard serial I/O mode” pp.276–277, Table EE-2 p.278, Fig. EE-2 p.279,
and ID section pp.283–284.

- Reset release with M0=H and M1=L starts factory boot-ROM code. Normal mode
  uses M0=L (**PROVEN**).
- VCC is 3.3±0.3 V and FVCC 3.0–3.6 V; RESET must remain low for at least 20
  XIN cycles (**PROVEN**, Table EE-1).
- UART1 pins implement four-wire synchronous serial: CLK10 input, RxD10 into
  MCU, TxD10 CMOS output, RTS10/BUSY output (**PROVEN**, pp.276–277).
- Pin aliases from Fig. AA-1 are P65/CLK10 pin 40, P66/RXD10 pin 39,
  P67/TXD10 pin 38, P64/RTS10 pin 41, and FVCC pin 67 (**PROVEN**).
- Data is eight-bit, LSB-first; input samples on rising CLK and output changes
  on falling CLK. BUSY=L means ready and H means active (**PROVEN**, p.277).
- Table EE-2 includes user page read `FFh`: address middle/high then 256 bytes.
  It also includes page program, block erase, status, ID check, download,
  version, boot-ROM output, and read-check commands (**PROVEN**).
- Standard serial mode can reprogram only user ROM; boot ROM cannot be
  reprogrammed there. Factory boot ROM can itself be replaced only in parallel
  mode (**PROVEN**).

Therefore the complete 128-KiB user flash is **theoretically readable without
erase/program commands**, page by page, if the boot ROM remains factory-like
and authentication succeeds. Entering this mode on an assembled motherboard
still requires reset/mode/FVCC manipulation and is not operationally passive.

The datasheet specifies protocol behavior, not safe in-circuit motherboard
access, bus isolation, or Toshiba board contention. No physical procedure is
authorized here.

## 9. Seven-byte ID and protection

### Serial ID

For a nonblank device, commands from the programmer are not accepted until the
seven-byte ID matches (pp.276, 283–284). ID bytes are stored at:

`FFFDFh, FFFE3h, FFFEBh, FFFEFh, FFFF3h, FFFF7h, FFFFBh`.

The ID-check command F5h is accepted while unmatched, as are status 70h and
version FBh; page read FFh is explicitly “Not acceptable” when unmatched
(Table EE-2 p.278). Thus a correct ID is required to dump a programmed,
nonblank user ROM through standard serial mode. No authentic M200 ID is known.

An incorrect ID is documented to leave subsequent commands unaccepted. The K9
datasheet does **not** state that mismatch erases flash. Nevertheless, physical
entry and command mistakes remain unsafe because destructive commands coexist
in the same protocol. No brute force is proposed.

### Parallel ROM protection

ROM code protect is separate. Byte FFFFFh contains level-1/level-2 protect and
reset pairs (Fig. BB-6 pp.271–272). Enabling a level blocks parallel read and
modification; level 2 also blocks shipment-tester read. The reset pair can
remove protection only when rewritten through serial/other mode, which itself
changes flash. Its physical state cannot be determined safely from the current
evidence without an authenticated/read-capable path. The datasheet does not
say boot ROM bypasses user ID or ROM protection.

## 10. BIOS/SMM acquisition paths

Static evidence retained from authentic BIOS/service binaries:

- INT15 `AX=44FE/44FF` dispatches to Toshiba SMM using `OUT B2h,C0h`
  (**PROVEN**).
- Selectors observed include 16h audio/power, 2Bh DMI, 57h Intel microcode/FWH,
  60h platform settings, and 62h model/UUID/serial data (**PROVEN by utility
  machine code**).
- `DMIR` reads DMI; `DMIW`, `CHGMC`, and `MCUD` include explicit write/program
  operations to platform/FWH data. These are not EC-flash evidence.
- No analyzed selector exposes arbitrary K9 program memory, an EC flash
  checksum-to-bytes conversion, K9 programming-mode entry, or a 128-KiB image.

A verify/checksum primitive would not itself be a dump primitive. Current
verdict: software-only DOS/BIOS/SMM EC dump path **UNKNOWN / not proven**.

## 11. Authentic BIOS 62h/66h command inventory

| Command | Request | Response | Context/callsite | Interpretation |
|---|---|---|---|---|
| 90h | one index | one byte | module 3 `0030:455D/457A/4584`; F107; SYSCNF | indexed Toshiba EC read — **PROVEN**; no linear ROM read |
| 91h | index,value | no payload proven; IBF acceptance waits | module 3 `0030:468D/46A6`; F108 initialization | indexed write — **PROVEN**, state-changing |
| 9Eh | begins collection, followed by data | none immediately | BIOS file 7C1B5 (F002), 7F5FD (F006) | Toshiba packet framing — **PROVEN** |
| 9Fh | terminates/processes collection | length then payload | BIOS file 7C1D3–7C213, 7F709–7F739 | Toshiba security/model response — **PROVEN framing**, algorithm **UNKNOWN** |

Observed 90h indices include 05h, 0Dh, 26h, C0h, C1h during cold POST and a
larger configuration/version-oriented set in `SYSCNF.COM`. Observed 91h writes
include `8C:01`, `8C:02`, `2E:70`, `2C:01`, and `27:05`. No command in the
inventory carries a 17/20-bit program address or streams K9 flash pages.

## 12. F002 re-analysis

1. **LPC silicon — PROVEN:** an enabled programmable K9 LPC channel decodes
   62h/66h as A2-low/high. Host writes fill DBBIN, set IBF, and latch XA2. EC
   reads clear IBF. EC writes DBBOUT and sets OBF. Host 66h reads DBBSTS; host
   62h reads DBBOUT and clears OBF.
2. **Toshiba framing — PROVEN from BIOS:** command `9E`, data `11 10`, command
   `9F`; firmware polls IBF before successive writes, then polls OBF and treats
   the first byte as length. Zero length retries 9F.
3. **Security/model algorithm — UNKNOWN:** request interpretation, authentic
   length/payload, state dependencies, and final model byte. Only the BIOS
   acceptance constraint (bit 7 clear, low nibble 5) is known. Synthetic
   `01 05` and `01 01` are explicitly non-evidence.

What is missing is Toshiba EC firmware policy/backend behavior, not the basic
host mailbox.

## 13. Physical acquisition safety

| Method | Classification | Reason |
|---|---|---|
| Proven DOS/BIOS/SMM arbitrary read | READ-ONLY if found; currently UNKNOWN | no such selector/command proven |
| Existing documented EC host command | potentially read-only per command; dump use UNKNOWN | 90h indexes configuration only; undocumented probing forbidden |
| Standard serial boot-ROM page read | command itself READ-ONLY, setup state-changing/risky | needs reset, M0/M1, FVCC, correct ID, board isolation; erase/program commands share interface |
| Parallel mode/programmer | destructive risk | requires mode/power control; ROM protection; supports erase/program |
| External programmer in circuit | destructive/contention/backfeed risk | motherboard nets and power domains may drive each other |
| Passive LPC capture | READ-ONLY/passive | reveals host-visible transactions/responses, not complete firmware |

No mode-pin, reset, or FVCC operation should be attempted on the motherboard
without a separate schematic-level isolation and power-domain study.

## 14. K5 → K9 correction matrix

| Claim | Old source | K9 evidence | New verdict | Action |
|---|---|---|---|---|
| Physical EC is M306K5F8LRP | board-doc interpretation/K5 sheet | physical marking M306K9FCLRP | DISPROVEN | Correct all names/docs |
| Family is M16C/6K7/K5 | K5 sheet | K9 title/part list pp.1,8 | DISPROVEN | Use M16C/6K9 |
| 144-pin package | K5 sheet | K9 Table AA-2/package p.290 | PROVEN unchanged | Cite K9 |
| K5 pinout applies | K5 sheet | K9 Fig. AA-1 pp.4–5 and pin descriptions pp.10–12 | DISPROVEN as evidence | Rebuild every physical-pin claim from K9 |
| RAM 3 KiB | K5 | K9 Table AA-2 p.8: 5 KiB | DISPROVEN | Correct |
| user flash 64 KiB | K5 | K9 pp.3,8,260: 128 KiB | DISPROVEN | Correct acquisition sizes |
| K5 split user ranges | K5 | K9 E0000h–FFFFFh | DISPROVEN | Use contiguous K9 range |
| factory 4-KiB boot ROM | K5 family | K9 pp.259–260,273 | PROVEN independently | Cite K9 |
| four LPC channels | K5-family assumption | K9 p.149/Table AA-1 | PROVEN independently | Retain |
| DBBIN/DBBOUT lifecycle | K5 | K9 pp.152,159 | PROVEN independently | Retain |
| DBBSTS 0/1/3 and U flags | K5 | K9 Fig. GF-4 p.154 | PROVEN independently | Retain |
| XA2 reflects host A2 | K5 | K9 pp.149,152 | PROVEN for channels 1–3 | Clarify channel |
| 62/66 fixed silicon pair | inference | K9: only 60/64 fixed; 1–3 programmable | DISPROVEN as intrinsic | Model board-configured channel |
| IBF/OBF ownership | K5 | K9 pp.152,159–161 | PROVEN independently | Retain |
| three I²C channels | not established | K9 pp.1,8 | PROVEN | Update topology |
| normal/program voltage follows K5 | K5 | K9 pp.3,10,259,275: VCC/FVCC 3.0–3.6 V | DISPROVEN as K9 evidence | Use K9 only |
| program supply behavior | K5 M2 assumptions | K9 uses FVCC 3.0–3.6 V | DISPROVEN for physical part | Never apply K5 wiring |
| standard serial M0=H/M1=L | K5 family | K9 pp.261,275–276 | PROVEN independently | Retain only as research |
| 7-byte ID | K5 | K9 pp.272,276,283–284 | PROVEN independently | Correct K9 addresses |
| parallel ROM protection | K5 | K9 Fig. BB-6 pp.271–272 | PROVEN independently | Keep separate from serial ID |
| serial page reads imply easy dump | K5 | K9 Table EE-2: read blocked if ID unmatched | DISPROVEN | State “theoretical with ID” |
| full firmware dump is non-destructively accessible in circuit | K5-based feasibility inference | K9 has read-only FFh command but requires boot-mode wiring and matching ID | DISPROVEN operationally; theoretical only | Prefer passive capture/software research |
| 0x70 is silicon reset status | QEMU board shortcut | K9 DBBSTS reset 00h; BIOS requires U6:U4 at F002 | DISPROVEN as silicon, STRONG as Toshiba runtime state | Move to backend initialization |
| reject DBBOUT write while OBF=1 | QEMU defensive API | K9 does not define full-latch case | UNSUPPORTED / UNKNOWN | Do not change without evidence |
| BH represents physical delay | QEMU mechanism | K9 gives no firmware-service latency | DISPROVEN if claimed | Keep as scheduling abstraction only |

## 15. Recommended QEMU changes (not implemented here)

1. Rename K5 comments and documentation to M306K9FCLRP or a generic K9 LPC
   mailbox channel.
2. State explicitly that the board maps one programmable LPC channel to
   62h/66h; do not call it channel 0.
3. Mark DBBOUT-while-OBF and DBBIN-while-IBF behavior explicitly UNKNOWN. Do
   not replace the current defensive behavior with guessed overwrite semantics.
4. Remove wording that K9 explicitly defines host-overwrite behavior while IBF
   is set; retain overwrite only as the best single-latch interpretation.
5. Move `0x70` entirely to a future Toshiba backend initialization stage and
   keep silicon reset 00h. Preserve the BIOS-proven F002 requirement while not
   claiming it exists before Toshiba firmware initializes DBBSTS.
6. Preserve zero invented latency and the no-response default backend.

Unused LPC channels, IBF/OBE interrupt routing, LPCCON, and programmable address
registers are not needed merely to model the observed POST endpoint. Add them
only when firmware execution or other Toshiba ports require them.

## 16. Recommended firmware-acquisition path

The K9 serial interface proves a full read is technically possible only after
successful seven-byte authentication. It does not make in-circuit acquisition
safe. With no known ID and no software dump primitive, the highest-value safe
path remains passive observation of the already-executed F002 transaction.
That obtains an authentic test vector without reset, mode, FVCC, command, or
flash risk, though it cannot recover the general algorithm.

## 17. Remaining unknowns

- Which programmable LPC channel Toshiba maps to 62h/66h and its EC-side setup.
- Authentic DBBSTS U flags after Toshiba firmware initialization.
- Physical IBF/OBF service latency and overwrite behavior under protocol abuse.
- Complete K9 EC image, seven-byte ID, boot-ROM provenance, and ROM protect
  state.
- Meaning/response algorithms for Toshiba commands 90/91/9E/9F.
- Whether any still-unidentified Toshiba ASP package contains K9 firmware.
- Any software-only authenticated read/verify interface.

## 18. Mandatory final verdicts

**A. Is M306K9FCLRP now proven to be the physical M200 EC? — YES, PROVEN by
physical chip marking.**

**B. Exact documented user ROM — 128 KiB, E0000h–FFFFFh inclusive, PROVEN.**

**C. Does K9 independently confirm DBBIN/DBBOUT/IBF/OBF? — YES, PROVEN in
pp.152, 154 and 158–161.**

**D. Does it independently confirm XA2? — YES.** For programmable channels
1–3, A2 is not decoded and the host write's A2 is latched into XA2. This proves
the 62h/66h distinction once the channel base is configured.

**E. Wrong/unsupported POST-001A behavior:** K5 naming; presenting 62h/66h as
an intrinsic rather than board-configured channel; treating DBBOUT writes while
OBF=1 and host writes while IBF=1 as defined when both edge cases are UNKNOWN;
definite latch data reset values; and placing BIOS-supported 0x70 directly in
silicon reset rather than Toshiba firmware initialization. Missing
channel enable/address and interrupts are incompleteness, not current F002
fabrication.

**F. Status 0x70 — not a documented silicon reset state.** K9 DBBSTS resets to
00h. It is a **PROVEN BIOS-required** F002 status pattern and therefore a
**STRONG INFERENCE Toshiba firmware runtime state**; only QEMU's placement of
it directly in device reset is unsupported.

**G. Can complete user firmware theoretically be read through documented
serial I/O? — YES, page read covers user flash, if factory boot serial support
exists and authentication succeeds.**

**H. Does the seven-byte ID prevent read on a programmed device? — YES.** Page
read is not accepted while ID is unmatched on nonblank flash.

**I. Proven software-only DOS/BIOS/SMM dump path? — NO; none has been proven.**

**J. Credible local M306K9 image? — NO validated candidate has been found.**

**K. Missing for authentic F002:** the Toshiba EC firmware packet handler and
security/model algorithm, including authentic response length, payload, and
state dependencies. Silicon mailbox framing alone cannot supply it.

**L. Single safest next step:** perform the already designed strictly passive,
read-only LPC capture of the physical BIOS's natural `9E 11 10 9F` exchange
and recover the authentic DBBOUT response; do not issue any new EC command or
enter programming mode.

## Primary and supporting sources

[^k9]: Renesas, *M16C/6K9 Group — Single-Chip 16-Bit CMOS Microcomputer*,
    `REJ03B0041-0100Z`, Rev.1.00, 2003-06-06. Local source:
    `m200-data/M200DATASHEETS/M16C6K9.PDF`. This is the sole silicon authority
    used for K9 claims.

- Authentic Toshiba M200 BIOS payload:
  `m200-data/TOSHIBA_M200_BIOS_V180_AUTHENTIC.BIN`; BIOS command cross-reference
  retained in `scratch/ec_re/ec_command_inventory.tsv`.
- Toshiba M200 service media and static utility analyses in
  `docs/research/m200-recovery-floppy-ec-analysis.md` and
  `docs/research/m200-diagnostic-environment-evidence-audit.md`.
- Physical M200 part marking supplied by the project owner.
