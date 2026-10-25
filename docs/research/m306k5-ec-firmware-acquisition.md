# Toshiba Portégé M200 M306K5 EC Firmware Acquisition Feasibility

## Executive verdict

No software-only, read-only path to the complete Toshiba EC firmware is proven in the available material. The exact M306K5 silicon does provide a non-destructive 256-byte page-read operation in standard serial-I/O programming mode, but a nonblank part accepts page-read only after its seven-byte ID code has matched. Entering that mode also requires reset-time mode-pin control and a synchronous UART1 wiring path. It is therefore **not** an appropriate first experiment on the motherboard.

The strongest acquisition lead is not an undocumented host command. Toshiba's own POST documentation names `CHGFIRMA.EXE` as the EC/KBC rewrite program loaded by the BIOS recovery path. None of the seven locally held public M200 BIOS-update floppies (versions 1.10, 1.20, 1.30, 1.40, 1.60, 1.70, and 1.80) contains that file; they contain `CHGBIOSA.EXE` and `BIO0009T.COM`. Locating an original Toshiba service/recovery disk containing `CHGFIRMA.EXE` is the safest and highest-information next step.

Evidence labels in this report mean: **PROVEN** = direct primary-document or authentic-machine-code evidence; **STRONG INFERENCE** = multiple consistent primary observations; **HYPOTHESIS** = plausible but not demonstrated; **UNKNOWN** = evidence does not decide; **DISPROVEN** = contradicted by primary evidence.

## 1. Exact device and memory architecture

The M200 maintenance documentation identifies IC3200 as a Mitsubishi M306K5F8LRP EC/KBC. It is a 144-pin 144PFB-A plastic QFP member of the M16C/6K family with an LPC host interface. The exact-part differences table—not the M306K7 example memory map—gives the M306K5 layout.^1

| Property | M306K5F8LRP result | Evidence |
|---|---:|---|
| CPU | M16C-family 16-bit MCU | **PROVEN**, group datasheet |
| Package | 144-pin 144PFB-A QFP | **PROVEN**, description/product tables |
| User flash | 64 KiB total | **PROVEN**, exact-part address ranges |
| User-ROM ranges | `0E8000h–0F5FFFh` (56 KiB) and `0FE000h–0FFFFFh` (8 KiB) | **PROVEN**, exact-part differences table |
| Boot ROM, parallel-I/O view | `0FE000h–0FFFFFh` (8 KiB) | **PROVEN for K5**, exact-part differences table |
| Boot ROM, CPU-rewrite/standard-serial view | `0DE000h–0DFFFFh` (8 KiB) | **PROVEN for K5**, exact-part differences table |
| Internal RAM | 3 KiB, group map beginning at `00400h`; shared example ends at `00FFFh` | **STRONG INFERENCE**: common 6K7 specification and no K5 RAM exception is listed; the product table explicitly enumerates K7 only |
| SFR | `00000h–003FFh` | **PROVEN**, group memory map |
| Fixed vectors | `0FFFDCh–0FFFFFh` | **PROVEN**, group memory map |

The manual's generic flash chapter illustrates the M306K7's 4 KiB boot block. That figure must not replace the exact K5 differences table, which gives an 8 KiB boot mapping. The K5 also differs electrically: program/erase uses an M2 supply input specified at 4.5–5.25 V, while normal VCC is 3.0–3.6 V.^1

The Toshiba program is therefore expected to reside in internal NEW DINOR user flash. The board documentation shows separate FWH, SPD, clock-generator, and thermal devices, but no external program memory assigned to the EC. External storage of EC configuration is **UNKNOWN**; external storage of the executing EC program is unsupported by current evidence.

## 2. Flash operating modes and read capability

The exact documentation lists three flash modes:^1

| Mode | Reader/executor | Read capability | Entry / constraints | Persistent effect of read |
|---|---|---|---|---|
| Normal single-chip execution | M306 CPU | Ordinary instruction/data fetches | Normal reset mode | None |
| CPU rewrite | M306 CPU executing control code from RAM/boot context | Read-array command exists | Flash cannot be fetched normally while rewrite commands execute; only user area is reprogrammable | Read-array itself does not program/erase |
| Parallel I/O | External programmer | Read, program, erase | Programmer and mode/power wiring | Read is non-programming, but ROM code protect may block it |
| Standard serial I/O | Factory boot-ROM controller over synchronous UART1 | Page read, status, ID check, version, boot-ROM output; also destructive commands | M0=H, M1=L at reset; CLK10/RxD10/TxD10/RTS10; nonblank user flash requires matching ID | Page read is non-programming; mode entry changes volatile execution state |

In standard serial mode, command `FFh` reads one 256-byte page. The second and third bytes carry address bits A8–A15 and A16–A23; 256 bytes then leave TxD10. Table EE-2 marks page read “not acceptable” when the ID is unmatched. `FCh` reads a 256-byte page of boot ROM but is also not accepted before ID verification. `FBh` returns the boot control program's eight-character version and is accepted without ID verification.^1

This proves a *technical* non-destructive read primitive, not safe in-circuit access. The motherboard's mode-pin strapping, isolation from other drivers, M2 treatment, and serial test-point routing are not established.

## 3. Protection mechanisms

### 3.1 Parallel ROM-code protection

The user byte at `0FFFFFh` contains ROMCP/ROMCR fields. If either protection pair enables protection, parallel-I/O readout and modification are inhibited. Level 2 additionally blocks shipment-tester readout. The datasheet says the reset bits cannot be changed in parallel mode; changing them requires serial or another mode and would modify flash. No such change is permissible here.^1

### 3.2 Standard-serial ID check

A nonblank part compares seven supplied ID bytes with flash bytes at `0FFFDFh`, `0FFFE3h`, `0FFFEBh`, `0FFFEFh`, `0FFFF3h`, `0FFFF7h`, and `0FFFFBh`. If they do not match, commands other than those explicitly marked acceptable are not accepted. Page read and boot-ROM output are blocked; status, ID-check, and boot-version queries remain accepted.^1

The datasheet says an incorrect ID causes commands not to be accepted. It does **not** say that an incorrect ID erases the device. Nevertheless, brute-forcing IDs is excluded: it is unsupported, unnecessarily exercises programming mode, and cannot be declared electrically safe in-circuit.

### 3.3 Protection state on the physical M200

The seven ID bytes and ROMCP byte are themselves in protected user flash. Their values have not been observed. Consequently, whether this M200 permits serial or parallel readout is **UNKNOWN**. Protection can be diagnosed partly through serial status/acceptance behavior, but doing so would require entering programming mode; that is outside this read-only study.

## 4. Authentic BIOS host-command inventory

The reproducible inventory is [scratch/ec_re/ec_command_inventory.tsv](../../scratch/ec_re/ec_command_inventory.tsv). It distinguishes BIOS machine code from QEMU diagnostics.

### 4.1 F002 security transaction

At BIOS file offsets `0x7C1B5–0x7C213` (runtime `F000:C1B5–C213`), authentic x86 code executes:

```text
OUT 66h,9Eh
OUT 62h,11h
OUT 62h,10h
OUT 66h,9Fh
wait OBF; IN 62h                    ; response length
repeat wait OBF; IN 62h             ; payload
```

The send helpers at `F000:F32E/F34D` wait for IBF clear; `F000:F36C` waits for OBF. The response length must be nonzero. The final payload byte must have bit 7 clear and low nibble 5. Nothing in this code identifies `01 05` as the authentic response; that value existed only in diagnostic QEMU code.

### 4.2 F006 signature transaction

At BIOS file offsets `0x7F5FD–0x7F739`, command `9Eh` begins a request whose first byte is `18h`, followed by computed hash/state bytes and eight bytes from a firmware structure, then a zero byte. Command `9Fh` starts the response. The BIOS accepts a nonzero response length and later requires a returned payload byte equal to `01h`. This is a security/signature protocol, not a documented M306 flash command.

### 4.3 Indexed commands 90h and 91h

Authentic BIOS and `SYSCNF.COM` prove `90h` is an indexed one-byte read and `91h` an indexed write. Known POST reads include indices `05h`, `0Dh`, `26h`, `C0h`, and `C1h`; known writes include `8C←01`, `8C←02`, `2E←70`, `2C←01`, and `27←05`. `SYSCNF.COM` contains a larger fixed read table and displays “Embedded Controller Version”, proving a version/configuration query path. Neither code base constructs a linear EC flash address or streams 64 KiB.

The command inventory is substantial but not exhaustive: dynamically constructed indices and code in compressed/unreached modules remain possible. No command is presently proven to return EC ROM bytes or a full-ROM checksum.

## 5. Toshiba INT 15h / SMM audit

Authentic BIOS code dispatches AH=`44h`; AX=`44FEh/44FFh` ultimately calls the routine at BIOS file offset `0x244B8`, which executes `OUT B2h,C0h` and enters SMM. Success is returned as AH=0/CF clear; errors set CF.^2

Observed utility call sites establish selector-specific functions—not a universal arbitrary-memory API:

| Selector | Proven use | Direction/safety |
|---:|---|---|
| `16h` | Audio subsystem power control | Query plus state-changing set |
| `2Bh` | DMI data read/write | Read and persistent write variants |
| `57h` | BIOS microcode/table flash service | Query plus persistent programming |
| `60h` | Hardware configuration/SpeedStep flags | Query plus persistent set |
| `62h` | Model/serial/UUID information | Read path proven; some utilities also use related write services |
| `65h` | Microcode header/status | Query |
| `70h` | BIOS capability query | Query |
| `3000B10xh` | PCI configuration wrappers | Read/write depending subfunction |

No observed selector accepts an M306 address and returns EC flash data. Selector `57h` targets the host FWH/CPU-update area, not M306 flash. The presence of “verify” or “check” behavior does not imply that compared bytes are exposed to the caller; no oracle suitable for reconstructing EC contents is proven.

**Verdict:** an INT15/SMM EC dump is **UNKNOWN**, with no positive primitive found. Calling `44FFh` is explicitly excluded because known functions can modify persistent state.

## 6. Diagnostic and update utility audit

The prior byte-level utility disassemblies were re-used as reproducible supporting evidence, with their claims checked against authentic instruction listings.^2

| Artifact | Classification | EC acquisition finding |
|---|---|---|
| `SYSCNF.COM` | READ-ONLY for the examined EC path | Direct `90h` indexed reads and version display; no ROM stream |
| `KBDC.COM` | READ-ONLY keyboard test | No `62h/66h` EC security protocol; **DISPROVEN** as EC dumper |
| `DMIR.COM`, `HWCINF.COM` | READ-ONLY interfaces | DMI/system identity, not EC flash |
| `DMIW.COM`, `SETUUID.COM`, `CHGMC.COM`, `MCUD.COM` | WRITE/PROGRAM | Persistent DMI/FWH operations through SMM; unsafe and not EC dumpers |
| `FLASH.EXE` | WRITE/PROGRAM | Wacom ISD-V4 updater over serial; not M306K5 |
| `V4S_004A.HEX`, `V4S_0055.HEX` | Firmware data, unrelated | Header/tail identify Sanyo LC87F1564; **DISPROVEN** as EC firmware |
| `CHGBIOSA.EXE` / `BIO0009T.COM` | BIOS/FWH update | Present on every inspected BIOS-update floppy; no discrete EC payload identified |
| `CHGFIRMA.EXE` | UNKNOWN / not available | Named by Toshiba POST/recovery documentation as EC/KBC rewrite executable; absent locally |

No utility was executed on physical hardware.

## 7. Search for an embedded EC image

The local-artifact results are recorded in [scratch/ec_re/local_artifact_inventory.tsv](../../scratch/ec_re/local_artifact_inventory.tsv) and [scratch/ec_re/artifact_string_hits.txt](../../scratch/ec_re/artifact_string_hits.txt).

The 512 KiB physical FWH dump and extracted version-1.80 BIOS payload were searched for M306/M16C/EC-program identifiers and inspected at aligned 64 KiB boundaries for plausible standalone vector endings. Toshiba update executables and all seven floppy images were container-listed. No discrete 64 KiB M306K5 payload, M16C vector image, `CHGFIRMA.EXE`, or M306 serial-programming component was identified.

This is not proof that no compressed or transformed EC payload exists inside an unidentified BIOS module. Therefore the strict answer is **UNKNOWN (no candidate identified)**, not “proven absent.” The two large HEX files are positively excluded because their own headers and vectors identify the Wacom controller.

Archival web research found official Dynabook M200 BIOS/support records and the same public BIOS generations, but no original EC/KBC service package. The official maintenance manual independently confirms the M306K5 part and EC/KBC rewrite stage.^3

## 8. Can DOS dump the EC?

| Proposed path | Verdict | Reason |
|---|---|---|
| DOS → INT15 HCI → SMM → EC bytes | **UNKNOWN** | SMM gate is proven, but no EC-address/read selector is proven |
| DOS → ports 62h/66h → Toshiba EC command → bytes | **UNKNOWN** | Indexed configuration reads exist; no arbitrary ROM-read command/address protocol exists in evidence |
| DOS → BIOS verify service → infer bytes | **UNPROVEN** | No EC verify selector or useful comparison oracle has been identified |
| DOS → enter M306 standard serial mode | Not software-only | Requires reset-time mode pins and separate synchronous serial wiring |

Accordingly an `ECDUMP.COM` design cannot presently be specified without inventing an undocumented command. A safe DOS program could only inventory already-proven read indices and timing, which would improve behavioral emulation but would not acquire firmware.

## 9. Standard-serial read feasibility

The exact documented signal set for standard serial mode is:

| MCU function | Direction in programming mode | 144PFB-A pin | Evidence / board mapping |
|---|---|---:|---|
| P64 / RTS10 / BUSY | MCU output | 41 | Datasheet package diagram; motherboard test point unknown |
| P65 / CLK10 / SCLK | MCU input | 40 | Datasheet package diagram; motherboard test point unknown |
| P66 / RXD10 | MCU input | 39 | Datasheet package diagram; motherboard test point unknown |
| P67 / TXD10 | MCU output | 38 | Datasheet package diagram; motherboard test point unknown |
| M0 | mode input | package diagram identifies signal; exact K5 board access unknown | Must be high at reset |
| M1 | mode input | package diagram identifies signal; exact K5 board access unknown | Must be low at reset |
| RESET | input | package diagram identifies signal; exact board access unknown | Low for at least 20 XIN clocks, then release |
| XIN/XOUT | clock | package diagram identifies signals | Existing oscillator may be used only if mode circuitry permits |
| M2 | K5 program/erase supply | exact accessible point unknown | 4.5–5.25 V is specified for program/erase; read-only requirement is not sufficiently resolved |

Pins 38–41 are common 144PFB-A UART1 pins in the exact group datasheet. The K5-specific differences table changes P84 into M2 but does not list UART1 pin relocation; this pin assignment is a **STRONG INFERENCE**, not a verified motherboard net map.

If the seven-byte ID is known and electrical isolation is established, `FFh` page reads can theoretically acquire all 64 KiB of user flash without program or erase commands. If the ID is unknown or ROM protection applies, user-ROM acquisition is blocked. `FCh` can read boot ROM only after ID acceptance. Thus the programming interface answer is **YES IF PROTECTION ALLOWS**.

No mode-pin manipulation, reset wiring, ID trial, or programming-mode entry should be attempted based on this report.

## 10. Passive capture fallback

Passive LPC observation cannot recover the complete internal program, but it can capture authentic behavior needed by QEMU. The already continuity-verified breakout is:

| Via | LPC47N217 pin/signal |
|---:|---|
| 20 | LCLK |
| 15 | LFRAME# |
| 10 | LAD0 |
| 12 | LAD1 |
| 13 | LAD2 |
| 14 | LAD3 |

Capturing all six concurrently permits decoding host I/O writes to `62h/66h`, status reads, and returned DBBOUT bytes. The four-channel SDS814X HD cannot directly capture six signals in one acquisition. A passive high-impedance logic analyzer or FPGA/MCU synchronous sniffer with at least six 3.3-V-compatible inputs is the appropriate later instrument. It must never drive the bus.

Priority captures would be `9E 11 10 9F` and the F006 `9E ... 9F` request. Those reveal authentic responses and byte-service timing but not necessarily the underlying algorithm or complete firmware.

## 11. Safety classification

| Activity | Safety result |
|---|---|
| Offline BIOS/update/utility disassembly | **READ-ONLY / SAFE** |
| Listing and extracting update archives | **READ-ONLY / SAFE** |
| Running known `90h` reads on QEMU | **READ-ONLY to emulated state** |
| Issuing undocumented physical EC commands | **UNKNOWN / PROHIBITED** |
| Physical `91h` indexed writes | **STATE-CHANGING / PROHIBITED** |
| INT15 `44FFh` calls | **POTENTIALLY PERSISTENT / PROHIBITED** |
| M306 programming-mode entry | **VOLATILE MODE/RESET CHANGE plus electrical risk / DEFER** |
| Page read after safely established programming mode | **Logically non-programming**, but system-level safety not established |
| Passive LPC capture | **READ-ONLY if truly high impedance and voltage-compatible** |

## 12. Mandatory verdicts

**A. Is the complete EC firmware already present in any available file?**

**UNKNOWN.** No candidate was identified; compressed/transformed content cannot yet be exhaustively excluded. The Wacom HEX files are disproven.

**B. Can the complete EC firmware be dumped through normal 62h/66h host commands?**

**UNKNOWN.** No arbitrary-address or streaming ROM-read command is proven.

**C. Can Toshiba INT15/SMM provide a read-only EC firmware dump?**

**UNKNOWN.** The SMM gate and many selectors are proven, but none exposes EC program memory.

**D. Can a custom DOS program dump the EC without physical wiring?**

**NO WITH CURRENT EVIDENCE.** Designing one now would require an invented host command or SMM selector.

**E. Can the M306K5 programming interface read the firmware non-destructively?**

**YES IF PROTECTION ALLOWS.** Standard-serial `FFh` page read is documented and non-programming, but it requires a matching ID on a nonblank part and invasive mode/reset wiring.

**F. Is the physical M200 likely protected against this read?**

**UNKNOWN.** Neither ID bytes nor ROMCP state has been observed. A shipping security-sensitive EC plausibly uses protection, but that is only a hypothesis.

**G. Safest next experiment — exactly one.**

Acquire an original Toshiba M200 service/recovery disk containing the BIOS-named `CHGFIRMA.EXE` and analyze its files entirely offline: archive listing, hashes, payload carving, M16C vector validation, and disassembly. Do not execute it. This can reveal the authentic EC image and updater transport without touching the physical machine.

## Sources

1. Mitsubishi Electric, *M16C/6K7 Group Datasheet*, Rev. 1.0, especially pp. 2–7, 11–12, 145–159, and 250–280. Local source: `m200-data/M200DATASHEETS/M306K5F8LRP_M16C6K7.PDF`. Official current archive: [Renesas M16C/6K7 Group Datasheet](https://www.renesas.com/en/document/dst/m16c6k7-group-datasheet).
2. Toshiba M200 authentic BIOS and factory utilities: `m200-data/BACKUP_BIOS.BIN` (SHA-256 `b2e9367d…a571c`), `scratch/diag_re/bios_all.asm`, and utility disassemblies under `scratch/diag_re/`. Machine-code offsets are given inline. Supporting audit: [m200-diagnostic-environment-evidence-audit.md](m200-diagnostic-environment-evidence-audit.md).
3. Toshiba, *Portégé M200 Maintenance Manual*, document 960-457, especially hardware overview and D-port POST table pp. 1-7 and 2-22 onward. Local source: `m200-data/toshiba_portege_m200.pdf`. Archived copy: [Portégé M200 Maintenance Manual](https://macdat.net/files/pdf/toshiba/service_manual/portege_m200.pdf).
4. Dynabook/Toshiba, [Portégé M200 BIOS support note](https://support.dynabook.com/support/viewContentDetail?contentId=824179), documenting public BIOS-version requirements; no EC payload is offered on the located support page.
