# EC-RE-003 — M200 recovery/service floppy EC/KBC forensic analysis

Date: 2026-09-15  
Repository HEAD: `de8a7ca12e90fdf0803cdd28874ef23a044f3020`

## Executive finding

The available Toshiba Portege M200 service disk is a factory diagnostics and
initial-configuration disk, not the BIOS/EC recovery disk described by the
maintenance manual. It contains no `CHGFIRMA*`, no EC/KBC rewrite path, no
M306K5 firmware, and no EC-resident flash writer.

A separate original M200 BIOS-update floppy was also examined because it is
the repository's actual bootable recovery/update medium. It contains only
`CHGBIOSA.EXE` and `BIO0009T.COM`; its `AUTOEXEC.BAT` runs
`chgbiosa BIO0009T.COM`. It likewise contains no `CHGFIRMA`, external EC
payload, or service-script reference to one.

The only complete 64-KiB firmware images found on the service disk are the
two Intel-HEX digitizer images `V4S_004A.HEX` and `V4S_0055.HEX`. Both decoded
images identify their target as `LC87F1564`, consistent with the Wacom updater
and inconsistent with the M306K5 EC. Several utilities implement ordinary
indexed commands 90h/91h for EC configuration and version display, but none
implements commands 9Eh/9Fh, EC flash transfer, or ROM readback.

Accordingly, this media cannot supply an authentic POST-001A backend or the
F002/F006 responses.

## 1. Preserved media and filesystem inventory

### Primary service image

| Property | Value |
|---|---|
| Repository path | `m200-data/Toshiba_M200_AllBiosUpdates_and_DiagnosticTool/pom200t1-V1.5/pom200t150.IMA` |
| Byte size | 1,474,560 |
| SHA-256 | `e4c98f0e669c5563143e0ef597fa72481ddea46f537588a9a649124e9162efad` |
| Filesystem | FAT12, 2,880 × 512-byte sectors |
| BPB volume label | `NO NAME` |
| BPB serial | `180e2f49` as stored on disk (little-endian display convention may reverse it) |
| Active root files | 77 |
| Deleted root entries | none |

The image was never mounted read/write. Files were extracted with 7-Zip into
`scratch/ec_re003/extracted/service/`. The complete root listing, DOS
timestamps, sizes, and per-file hashes are in
`scratch/ec_re003/file_inventory.tsv`; image and extracted-file hashes are in
`scratch/ec_re003/SHA256SUMS`.

`m200-data/ToshibaPortegeM200_DiagnosticEnvorinment.img` is a later working
copy of the same disk. It has the same BPB identity and active file set but a
different SHA-256 (`1b652446...c2c0bc6`), 150 differing bytes, and one deleted
root entry `?MIINFO.TXT`. It was not treated as the pristine source.

### Separate BIOS-update image

| Property | Value |
|---|---|
| Repository path | `m200-data/toshibabiosfiles/Boot-1.44M.img` |
| Byte size | 1,474,560 |
| SHA-256 | `0e63872601e71c8d4ed42a70eca90fb8bddec80a9ba20223000f614da2044f2e` |
| Filesystem | FAT12 |
| BPB volume label | `NO NAME` |
| Relevant files | `CHGBIOSA.EXE`, `BIO0009T.COM`, `AUTOEXEC.BAT`, documentation |

The BIOS-update image has no deleted root entries and no textual occurrence
of `CHGFIRM`, `M306`, `M16C`, Mitsubishi, Renesas, or `EC/KBC`, including in
unallocated raw-image bytes discoverable by a literal search.

## 2. Exact artifact search

The following were searched in filenames, scripts, documentation, executable
strings, and raw floppy bytes: `CHGFIRMA`, `CHGFIRM`, `EC`, `KBC`, `EC/KBC`,
`firmware`, `rewrite`, `flash`, `M306`, `M306K5`, `Mitsubishi`, `Renesas`, and
`M16C`.

No active, hidden, or deleted root entry matches `CHGFIRM*`, `EC*.BIN`,
`EC*.ROM`, `EC*.DAT`, `KBC*.BIN`, or `KBC*.ROM`. FAT root-directory parsing
is preserved in `scratch/ec_re003/fat_root_entries.tsv`.

Potentially misleading files were resolved as follows:

| File | Actual role | Evidence | EC verdict |
|---|---|---|---|
| `CHGMC.COM` | Intel CPU microcode / CPU information-table flash utility | strings, `1.BAT`, and MSR-oriented prior disassembly | **DISPROVEN** |
| `FLASH.EXE` | Wacom digitizer firmware updater | executable strings and `1.BAT` invocation with `V4S_*.HEX` | **DISPROVEN** |
| `V4S_004A/0055.HEX` | Sanyo LC87F1564 digitizer firmware | decoded image marker `LC87F1564` at offset FFD0h | **DISPROVEN** |
| `FIREV.COM` | network/IEEE-1394-era diagnostic naming, not firmware | strings and test-oriented UI | **DISPROVEN** |
| `RSFIR.EXE`, `SNFIR.EXE` | FIR infrared send/receive tests | frame/checksum/transceiver strings | **DISPROVEN** |
| `KBDC.COM` | keyboard matrix/scan-code test | complete I/O audit: reads 60h/64h, no 62h/66h writes | **DISPROVEN** |
| `SYSCNF.COM` | system/EC configuration display | machine code for commands 90h/91h and version strings | no firmware payload |
| `TESTFF.EXE` | general Toshiba diagnostics including EC indexed read/write | x86 MZ code and menu strings | no firmware payload |
| `BIO0009T.COM` | M200 system BIOS update container | update README and sole `CHGBIOSA` workflow | no validated EC payload |

## 3. Service workflow reconstruction

`CONFIG.SYS` offers two boot configurations:

* `CE01 — Repair Main (T&D)`
* `CE0F — Repair initial config set`

`AUTOEXEC.BAT` dispatches the selected item. `MAIN.BAT` for CE01 runs
`ENFIR` then `TESTFF`, Toshiba's general diagnostics. CE0F reaches the menu in
`SUB01.BAT`:

1. initial configuration;
2. DVD region configuration (documented in the README although omitted from
   the shortened later menu text);
8. system configuration display;
9. EEPROM MAC/DMI check.

The initial-configuration path in `1.BAT` performs CPU-table selection,
microcode update, digitizer firmware check/update, DMI/UUID work, hardware
configuration, and DVD-region handling. The only firmware updater it invokes
is:

```text
FLASH V4S_004A.HEX
or
FLASH V4S_0055.HEX
```

That branch is explicitly labelled digitizer firmware. No script contains an
EC/KBC rewrite option or references another disk for EC firmware.

The separate BIOS-update floppy is even narrower:

```bat
@echo off
goto bios
:bios
chgbiosa BIO0009T.COM
```

Its Toshiba README enumerates all included update files and describes only
system BIOS installation. `CHGFIRMA` and an EC payload are absent from that
manifest and filesystem.

## 4. Embedded-blob analysis

Every COM/EXE/BIN/DAT/HEX file was inventoried. MZ-declared sizes were
compared with actual sizes to locate overlays; entropy, hashes, padding, and
candidate vector structures were examined. Results are recorded in
`scratch/ec_re003/blob_candidates.tsv` and
`scratch/ec_re003/mz_overlay_inventory.tsv`.

### The two 64-KiB images

Decoding the Intel HEX records produces:

| Source | Decoded size | SHA-256 | Identification |
|---|---:|---|---|
| `V4S_004A.HEX` | 65,536 | `0ca312182c943f884db80e17b220377cb5bf3d4a661b921ac052b7c97677fe56` | `LC87F1564` at FFD0h |
| `V4S_0055.HEX` | 65,536 | `815d8e3e3636d3a5dd0bab9716762fa6d862c2e6dbd2b5ffa21aeef13700d3a8` | `LC87F1564` at FFD0h |

Their exact 64-KiB size is coincidental with M306K5 user flash. The embedded
target string, Wacom updater, service-script data flow, and digitizer-labelled
branch positively identify them as unrelated controller firmware.

### Large executable overlays

`FLASH.EXE` has a 34,435-byte overlay and `unitchk.exe` a 22,514-byte overlay.
Both begin with the same compiler debug-directory signature and contain
Borland-style symbol names (`__cleanup`, `__checknull`, `DGROUP`, C-library
symbols). They are debug/symbol overlays, not M16C code or EC payloads.

`TESTFF.EXE` is 61,378 bytes with no overlay. It is ordinary 16-bit x86 code;
the apparent 62h/66h signatures resolve to its indexed EC read/write helpers.
There is no appended writer or 64-KiB image. Other large files are DOS system
files, x86 diagnostics, or the identified firmware above.

`BIO0009T.COM` has high entropy because it is a system-BIOS update container.
No standalone M16C vector region, M306 target marker, EC writer header, or
EC-update call path was validated. A compressed opaque component can never be
excluded from entropy alone, but there is no positive executable or data-flow
evidence that this BIOS package embeds EC firmware. It is therefore **UNKNOWN
internally but unsupported**, not classified as EC firmware.

## 5. Service-utility EC host protocol

No rewrite utility equivalent to CHGFIRMA exists on either floppy. Static
analysis nevertheless found two consumers of the normal host interface.

### `SYSCNF.COM`

Runtime offsets 1E71h/1E94h/1EB7h implement:

* wait on status 66h bit 1, then write a command to 66h;
* wait on bit 1, then write data to 62h;
* wait on status bit 0, then read data from 62h.

The higher-level routines implement command 90h as `command, index, response`
and command 91h as `command, index, value`. The utility reads a fixed table of
indices (`05, 37, 3E, 47, 4E, 81, 83, 84, 90..9E`), preserves values, performs
temporary configuration writes, reads `C0`, `C1`, `82`, `80`, `9F`, and `9A`,
then restores preserved fields. Its UI displays KBC, PSC, and Embedded
Controller versions. It never constructs a program-memory address or streams
firmware.

### `TESTFF.EXE`

Load-module offsets 4BB5h/4BDAh/4BFFh are equivalent IBF/OBF helpers. Routines
4B7Fh and 4C75h implement indexed command 90h reads; 4B4Fh and 4C53h implement
command 91h writes. A second inline implementation exists near 736Dh. These
support diagnostic “read/write specified address” functions, meaning an EC
configuration index—not arbitrary M306 program flash. There is no 9Eh/9Fh
packet path or flash-block protocol.

The cross-reference against authentic BIOS commands is in
`scratch/ec_re003/ec_command_cross_reference.tsv`.

## 6. Comparison with related Satellite 1800 CHGFIRMA

The related updater recovered in EC-RE-002 also polls 66h bit 1 for IBF and
bit 0 for OBF, but then uploads a 2,944-byte EC writer and transfers 32-byte
system/boot blocks with additive checksums and FAh/FEh acknowledgement.

No analogous evidence exists here:

* no `92/F0/F1` writer-upload sequence;
* no `B0/B1/B2` erase/write command sequence;
* no FAh/FEh ACK/retry loop associated with firmware blocks;
* no small M16C writer blob;
* no external `EC*.BIN`;
* no CHGFIRMA filename or UI strings.

This structural comparison is evidence of absence, not permission to assume
the K7 protocol would be identical on M306K5.

## 7. BIOS cross-check and F002/F006

Authentic M200 BIOS code at F000:C1B5–C1FA performs:

```text
CMD 9E
DATA 11
DATA 10
CMD 9F
wait OBF; read response length
wait OBF; read payload bytes
```

The F006 path constructs a longer 9Eh request and likewise finishes with
9Fh. Neither command byte occurs in a validated service-utility protocol path.
No firmware-side DBBIN/XA2/IBF handler exists on the disks, so the packet
parser and response algorithm cannot be recovered.

Commands 90h and 91h correlate structurally between BIOS and the service
utilities, including ordinary indexed EC configuration/version access. That
correlation proves only the host mailbox protocol. It offers no program-ROM
read primitive and no evidence that configuration index space aliases M306K5
flash.

No seven-byte constant was reported merely because it happened to occur in a
binary. No code/data flow invokes M16C standard-serial ID authentication, and
no standard-serial writer exists. Therefore no seven-byte serial ID was
found.

## 8. Why Toshiba documentation mentions CHGFIRMA

The authentic M200 maintenance manual's F009 recovery flow says the BIOS
resets the floppy controller, reads the FAT root directory, searches for
`CHGBIOSA.EXE` **or** `CHGFIRMA.EXE`, loads the selected EXE header, and
executes it. This describes a generic BIOS-key recovery loader. It does not
say that every diagnostic or public BIOS disk includes both utilities.

The evidence therefore indicates:

1. The service disk is diagnostic/factory media and was never intended to be
   the EC rewrite payload disk.
2. The public BIOS-update disk supplies the BIOS branch only.
3. An M200-specific EC/KBC rewrite distribution would have supplied
   `CHGFIRMA.EXE` and its model-specific companion payload separately in the
   floppy root, most plausibly as ASP/factory service media issued when an
   EC/KBC rewrite was required.

There is no script reference to a “second disk”; the separation is inferred
from the BIOS loader accepting alternative root files and the actual media
contents, not from an invented batch workflow.

## 9. Evidence matrix

| Conclusion | Level | Basis |
|---|---|---|
| Service image contains no CHGFIRMA entry | **PROVEN** | FAT root, raw bytes, extracted files |
| No deleted/hidden CHGFIRMA entry exists | **PROVEN** | all 224 FAT root slots parsed |
| Service workflow has no EC rewrite branch | **PROVEN** | complete BAT/INI/TXT reconstruction |
| BIOS update floppy contains only BIOS updater/payload | **PROVEN** | root files, AUTOEXEC, Toshiba README |
| V4S files are M306K5 firmware | **DISPROVEN** | decoded `LC87F1564` marker and Wacom data flow |
| TESTFF/SYSCNF can read arbitrary EC program flash | **DISPROVEN with current interface evidence** | only command-90 indexed one-byte accesses |
| BIO0009T secretly embeds compressed EC code | **UNKNOWN**, no supporting evidence | opaque BIOS container cannot be disproved by strings alone |
| An authentic M306K5 image is present on these media | **NO supported candidate** | exhaustive inventory and blob validation |
| F002/F006 algorithm can be recovered from these media | **DISPROVEN** | no handler/payload/updater |

## 10. Mandatory verdicts

**A. CHGFIRMA or equivalent on the M200 floppy: NO.** Neither service nor
BIOS-update floppy contains an EC rewrite equivalent.

**B. Authentic M306K5 firmware payload present: NO.** No validated payload is
present; the two 64-KiB images are positively identified as LC87F1564
digitizer firmware.

**C. EC-resident writer present: NO.** No writer blob or host upload protocol
comparable to the related updater exists.

**D. Seven-byte M306K5 ID found: NO.** No candidate has authentication
data-flow evidence.

**E. F002 9E/9F handler recovered: NO.** Only the x86 BIOS caller is present.

**F. Authentic F002 response recovered: NO.** In particular, `01 05` remains
diagnostic scaffolding, not physical evidence.

**G. Later F006 response recovered: NO.**

**H. Can POST-001A backend now be implemented authentically: NO.** The normal
90h/91h indexed interface is corroborated, but neither security response
algorithm is available.

**I. Where should the missing artifact come from?** The BIOS F009 loader and
related Toshiba packaging establish that it should be an M200-specific
`CHGFIRMA.EXE` plus companion EC firmware supplied on separate EC/KBC rewrite
service/recovery media, placed in the FAT root. It was not bundled with this
factory diagnostics disk or the public v1.80 BIOS disk.

**J. Exactly one safest next step:** search existing Toshiba ASP/service-media
archives offline for an M200/M205-specific package whose root manifest pairs
`CHGFIRMA.EXE` with `EC*.BIN`; acquire and hash the archive without executing
it. This directly targets the proven missing distribution while avoiding all
physical EC commands and programming-mode entry.

## Safety and repository state

No floppy was executed or mounted writable. No physical system was contacted.
No utility, EC command, serial mode, flash, CMOS, or NVRAM operation occurred.
No QEMU source was modified. No commit or push was made. All newly generated
material is confined to `docs/research/` and `scratch/ec_re003/`.

## Primary evidence

* Original service image: `m200-data/Toshiba_M200_AllBiosUpdates_and_DiagnosticTool/pom200t1-V1.5/pom200t150.IMA`
* Original BIOS-update image: `m200-data/toshibabiosfiles/Boot-1.44M.img`
* Toshiba M200 maintenance manual: `m200-data/toshiba_portege_m200.pdf`,
  section 2.4, table 2-3, F009h.
* Authentic M200 BIOS machine-code trace: documented in
  `docs/research/m200-diagnostic-environment-evidence-audit.md`.
* M306K5 primary datasheet:
  `m200-data/M200DATASHEETS/M306K5F8LRP_M16C6K7.PDF`.
