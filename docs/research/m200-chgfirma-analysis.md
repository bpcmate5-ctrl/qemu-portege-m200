# EC-RE-002 — Toshiba CHGFIRMA offline analysis

Date: 2026-09-10  
Repository HEAD during analysis: `de8a7ca12e90fdf0803cdd28874ef23a044f3020`

## Executive result

No Portege M200 copy of `CHGFIRMA.EXE` was found in the local M200 BIOS,
diagnostic, disk-image, or research collection, or in the historical packages
located online. A real Toshiba `CHGFIRMA.EXE` **was** recovered from the
related Satellite 1800 BIOS package `b1830.exe`. It is a DOS MZ executable,
version 2.10, dated 2001, and is accompanied by `EC30B100.BIN`.

The recovered updater is valuable protocol evidence, but it is not an M200
updater. Its payload identifies the target as `K7`, while the M200 uses an
M306K5F8LRP. The updater communicates with the running Toshiba EC through
ports 66h/62h. It does not directly bit-bang or invoke the documented M16C
serial-programming interface, and no seven-byte serial ID code was found.

The payload is structurally authentic EC firmware: the updater parses its
header, verifies its checksums, uploads a resident write program, and divides
it into a 64-KiB system image plus a 4-KiB boot image. The extracted images
also contain plausible M16C 20-bit vector entries. This does **not** establish
the M200 security-controller response algorithm; the related K7 payload must
never be loaded into or used as firmware for an M200.

## Search scope and provenance

### Local material

The search covered every file below `m200-data/`, including the extracted
M200 BIOS releases 1.10, 1.20, 1.30, 1.40, 1.60, 1.70, and 1.80, their ISO
images, the Toshiba diagnostic floppy, raw disk images, firmware dumps, ZIPs,
and extracted service utilities. Both filenames and binary strings were
searched for `CHGFIRM`, `CHGFIRMA`, `M306`, `M16C`, EC/KBC firmware terms,
and known related payload names.

All seven public M200 BIOS packages contain `CHGBIOSA.EXE` and
`BIO0009T.COM`; none contains `CHGFIRMA.EXE` or an external EC image. The M200
diagnostic set likewise has no `CHGFIRM*` file. In particular, the Wacom
`V4S_*.HEX` files and `FLASH.EXE` are not EC firmware, and the FireWire and
CPU-microcode utilities were not reclassified as EC tools.

The package inventory and hashes are in
`scratch/chgfirma/candidate_packages.tsv`.

### Historical search

The authentic M200 maintenance manual proves that Toshiba's recovery path
searches for and executes both `CHGBIOSA.EXE` and `CHGFIRMA.EXE`; it does not
contain either executable.[^m200-manual] Related Toshiba maintenance manuals
show the same recovery convention, including Satellite A40.[^a40-manual]

Two related-model package leads were found:

* A Satellite A40 report lists `CHGFIRMA.EXE` with `EC62NOBB.BIN`, but the
  package bytes were not recovered. This is secondary evidence only.[^a40]
* A Satellite 1800 `b1830.exe` package was recovered from a third-party
  historical driver mirror. The mirror identifies it as Toshiba BIOS v2.00
  and lists the same four files obtained from the archive.[^b1830]

The downloaded wrapper is not asserted to have come directly from a current
Toshiba server. Its embedded archive is internally consistent and all
artifacts are hashed below, but provenance remains “archived Toshiba package
via third-party mirror.” Nothing was executed.

An older Satellite 4060 package contains `EC05F12A.BIN`, but its published
archive listing has `CHGBIOSA.EXE`, not `CHGFIRMA.EXE`.[^4060] A contemporary
Satellite 4090 user report independently identifies `CHGFIRMA.EXE` as
Toshiba's “Change Firm” utility, but supplies no bytes.[^4090]

## Candidate package manifest

| Artifact | Model | Size | SHA-256 | Result |
|---|---:|---:|---|---|
| `b1830.exe` | Satellite 1800, BIOS v2.00 | 264,192 | `62099867b9e1e88c8c179a7cdd597757c687ea8af23b25d03dab419783f7a4df` | WinZip self-extractor containing updater and EC payload |
| `CHGFIRMA.EXE` | Satellite 1800 related model | 18,977 | `9d8a002949d5b8562f5c44bef5eb9d13fbaa823800cd6f426d5c358330e839ea` | DOS MZ, Toshiba CHGFIRM v2.10 |
| `CHGBIOSA.EXE` | Satellite 1800 | 30,644 | `eeb478673cb30aa6b2c3d5308cf78e2257776f5805b999b34b923b71e930ead2` | BIOS updater; not analyzed as EC updater |
| `EC30B100.BIN` | Satellite 1800 EC payload | 72,832 | `fb93de32c9f4791ab4053d2eff9d46d3416b44c351f7e8e542c68c8c78d72f19` | Structured K7 EC package |
| `BIOFC72T.COM` | Satellite 1800 BIOS image | 368,896 | `a9f722244197050448c12e027df19df89196107fa22dc07446ab44add55a0082` | System BIOS payload |

The recovered files are retained under `scratch/chgfirma/satellite1800/`.
They are research artifacts and are not proposed for commit.

## `CHGFIRMA.EXE` static analysis

### Format and identity

`CHGFIRMA.EXE` is a 16-bit DOS MZ program. Its resident strings state:

* `CHGFIRM (CHanGe FIRM Utility) V2.10`
* `(C)Copyright 1998,2001 Toshiba Corporation`
* expected filename form `ECxx????.BIN`, where `xx=[EC Type]`
* separate progress/error states for system and boot erase/write
* `FIRM name struct error`, checksum, mode, erase, and write errors

The complete load-module disassembly and string table are in
`scratch/chgfirma/analysis/chgfirma.ndis` and
`scratch/chgfirma/analysis/chgfirma.strings.txt`.

### Host interface

The key helper routines are shown using load-module offsets (file offset is
512 bytes greater because the MZ header is 0x200 bytes):

| Offset | Operation | Proven behavior |
|---:|---|---|
| `141Dh` | command write | poll `IN 66h` until status bit 1 clears, then `OUT 66h,AL` |
| `1438h` | data write | poll `IN 66h` until status bit 1 clears, then `OUT 62h,AL` |
| `1453h` | data read | poll `IN 66h` until status bit 0 sets, then `IN 62h` |

Thus the updater explicitly implements the same IBF/OBF ownership contract
used by the M200 architectural mailbox: bit 1 is input-buffer-full and bit 0
is output-buffer-full. There are no immediate `OUT B2h` instructions, and
the three `INT 15h` calls are platform queries, not the transport path. Two
calls use `AX=44FEh, BX=0070h` to read words corresponding to BIOS state at
F000:C020/C022 on one platform branch. The actual firmware transport remains
direct port 66h/62h I/O.

### Update protocol reconstructed from machine code

The observed protocol is a Toshiba mailbox protocol layered above the host
registers, not the M306 serial boot protocol:

1. `92h` is sent as a command, followed by data byte `00h`.
2. `F0h` starts a data stream containing the payload's resident write
   program. For the recovered format, the stream is 0xB8 paragraphs = 2,944
   bytes. `F1h` terminates it.
3. The updater waits for EC output `AAh` before proceeding.
4. System programming begins with command `B1h`, or `B2h` when a payload
   tag at offset +4 equals ASCII `ext `.
5. For every system block it sends `F0h`, 32 data bytes, and an additive
   checksum byte chosen so the total is `55h`, then sends `F1h`.
6. The EC returns `FAh` for accepted blocks. `FEh` requests retransmission;
   other values take the error path. Progress callbacks run every 64 blocks.
7. Boot-block programming uses command `B0h` and the same framed block,
   checksum, ACK, and retry scheme.
8. At completion the utility sends `FFh`, disables interrupts, halts, and
   requires power-down.

The strings distinguish erase and write failures for both system and boot
regions, but the x86 program does not directly issue M16C erase/program
opcodes. Those semantics are performed by the uploaded writer and EC-side
code.

No synthetic F002 responses (`01 05` or `01 01`) occur in the executable or
payload as an identified protocol action. The recovered update commands are
not the M200 BIOS security transaction `9E ... 9F`.

### Image validation and mapping

`EC30B100.BIN` has an 0x100-byte Toshiba container header. The version-2
fields consumed by `CHGFIRMA` establish, rather than merely suggest, this
layout:

| File range | Size | Role | SHA-256 |
|---|---:|---|---|
| `000100h..000C7Fh` | 2,944 | EC-resident write program | `8d105eb05e3de6329a91d10bd2887fd1d5064ea14f5492ac232caf81502fd5df` |
| `000C80h..010C7Fh` | 65,536 | system firmware | `9f31684ad01088c2220f32c6b09d18b5fa4f17a2194ef2d6723e05d35cbeb9ce` |
| `010C80h..011C7Fh` | 4,096 | boot block | `d305ca7d50eb79f9b8d217bd42f18b1c794ef435a17d0c614799414cbc433b75` |

The updater reads the header's 0x20-byte transfer size, 0x800 system-block
count, and 0x80 boot-block count, producing exactly 64 KiB plus 4 KiB. It
checks the header and each area with additive checksums before writing.

The container identifies itself as `V0.23  K7` and contains Toshiba firmware
version/date strings. The top of the extracted system image contains repeated
little-endian 20-bit vector values `000FF05Dh`; the boot image terminates in
similar vectors and a distinct `000FF010h` reset candidate. Combined with the
updater-derived boundaries, embedded version metadata, code-like regions,
and vector structure, this is **PROVEN** to be a Toshiba EC firmware package.
It is **not** proven to target M306K5 and its own `K7` marker is evidence
against using it as an M200 payload.

An M16C-aware disassembler was not available in the analysis environment.
Consequently, handler addresses for commands 90h, 91h, 9Eh, and 9Fh could not
be responsibly assigned. Literal-byte searches would produce many false
positives and were not used to claim handlers.

### ID code and protection handling

No seven-byte M306 serial ID constant or serial-programmer authentication
sequence was identified. The x86 updater never enters a direct serial mode;
it delegates flash operations to code transported through the running EC
mailbox. Any flash ID/protection knowledge could therefore be inside the
writer or boot firmware, not exposed as a seven-byte x86-side constant.

The recovered evidence proves erase/write capability, block checksums, and
EC acknowledgements. It does not prove a page-read command, arbitrary ROM
verification primitive, or non-destructive extraction path. Update success
does not imply readback access.

## Relevance to the M200/F002 algorithm

The utility contributes three reusable architectural facts:

1. Toshiba's production updater respects real DBBIN/IBF and DBBOUT/OBF
   ownership at ports 66h/62h.
2. Toshiba separated a generic x86-side transfer utility from model-specific
   external EC firmware.
3. A running EC can accept an uploaded writer and perform protected internal
   flash operations without the x86 directly using serial programming pins.

It does **not** reveal the authentic response to `9E 11 10 9F`. The recovered
K7 firmware may use a different MCU, board protocol, and security algorithm.
No command-9E/9F handler has yet been validated in it. Therefore it cannot be
used to create a QEMU F002 backend response.

## Evidence classification

| Finding | Classification |
|---|---|
| M200 BIOS recovery searches for `CHGFIRMA.EXE` | **PROVEN**, authentic maintenance manual |
| Public local M200 BIOS packages omit it | **PROVEN**, complete local file/archive scan |
| Recovered executable is Toshiba CHGFIRM v2.10 | **PROVEN**, executable bytes and strings |
| Recovered package is an M200 updater | **DISPROVEN/unsupported**; package is Satellite 1800 and payload says K7 |
| Transport uses 66h/62h IBF/OBF mailbox | **PROVEN**, x86 machine code |
| Transport uses SMM or direct serial mode | **DISPROVEN** for this executable's transfer path |
| `EC30B100.BIN` holds EC writer + system + boot code | **PROVEN**, updater data flow/header fields/vectors |
| Payload contains M200 F002 algorithm | **UNKNOWN**, no M16C handler disassembly and wrong target marker |
| Seven-byte serial ID is present | **NO IDENTIFIED ID**; absence cannot be proved inside opaque EC code |
| Update protocol supports readback | **UNKNOWN**, no read/verify path established |

## Final verdicts

**A. Was `CHGFIRMA.EXE` found? — YES.** SHA-256
`9d8a002949d5b8562f5c44bef5eb9d13fbaa823800cd6f426d5c358330e839ea`.

**B. Is it the M200 version? — Related-model version only.** The recovered
copy is from Satellite 1800 BIOS package `b1830.exe`; no exact M200 copy was
found.

**C. Was an M306K5 firmware payload found? — NO.** An authentic Toshiba EC
payload was found, but its container says K7. Calling it M306K5 firmware would
exceed the evidence.

**D. Was a seven-byte serial ID found? — NO.** No validated ID constant or
serial authentication exchange was identified.

**E. Does the updater use 62/66, SMM, or direct serial mode? — 62/66.** It
polls IBF/OBF and transfers a writer and firmware through the host EC mailbox.
Its `INT 15h` calls are platform queries; no `OUT B2h` or direct serial-mode
transport was found.

**F. Can this recover the authentic F002 response algorithm? — Not from the
present evidence.** It supplies a useful mailbox/update protocol and a
related K7 firmware specimen, but no validated M200/K5 command-9E/9F handler.

**G. Single safest next step — obtain an archived-original M200 recovery
floppy image and inspect it offline.** Specifically search its root directory
for `CHGFIRMA.EXE` and `EC*.BIN`, hash and extract them without booting or
executing the media. The M200 maintenance manual proves that this is the
expected distribution channel, making it higher-value and safer than probing
the physical EC or experimenting with undocumented commands.

## Safety statement

No recovered executable was run. No physical-machine command was issued. No
flash, CMOS, NVRAM, BIOS, or EC state was accessed or modified. No QEMU source
file was changed. All downloads and extraction products remain under
`scratch/chgfirma/` and are untracked research artifacts.

## Sources

[^m200-manual]: Toshiba, *Portege M200 Maintenance Manual*, recovery-flow description naming `CHGBIOSA.EXE` and `CHGFIRMA.EXE`, archived copy: https://macdat.net/files/pdf/toshiba/service_manual/portege_m200.pdf
[^a40-manual]: Toshiba, *Satellite A40 Series Maintenance Manual*, system-board troubleshooting/recovery flow: https://www.manualslib.com/manual/1020796/Toshiba-Satellite-A40-Series.html?page=80
[^a40]: Wim's BIOS forum, contemporary listing of files extracted from Toshiba A40 package, including `CHGFIRMA.EXE` and `EC62NOBB.BIN` (secondary evidence): https://www.wimsbios.com/forum/viewtopic.php?f=38&t=8858
[^b1830]: DriverGuide historical mirror, Toshiba Satellite 1800 `b1830.exe` package metadata and file manifest (third-party provenance): https://www.driverguide.com/driver/detail.php?driverid=61967
[^4060]: Elektroda archive listing for Toshiba Satellite 4060 package `bios_820.zip`, including `EC05F12A.BIN`: https://files.elektroda.pl/261616,bios_820.html
[^4090]: Geeks to Go, 2006 user report naming Toshiba `chgfirma.exe` “Change Firm” utility (secondary evidence): https://www.geekstogo.com/forum/topic/123139-virus/
