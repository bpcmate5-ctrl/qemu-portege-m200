# EC-RE-004 — M200/M205 EC/KBC rewrite distribution

Date: 2026-09-15  
Repository HEAD: `de8a7ca12e90fdf0803cdd28874ef23a044f3020`

## Executive result

The missing distribution was **not recovered**. Its identity is now narrowed
to a Toshiba model-specific BIOS/EC/KBC rewriting floppy used by the M200/M205
boot-block recovery path. The authentic M200 manual proves that the disk must
place `CHGFIRMA.EXE` in the FAT root, that holding **Tab** requests EC/KBC
update, and that POST F009 finds and executes the MZ program. The manual does
not identify a disk title, part number, volume label, updater version, payload
name, or command line.

The complete local M200 collection, seven public M200 BIOS generations and
their nested floppy images, the factory diagnostic floppy, raw research
images, and an archived M200 driver bundle contain no M200 `CHGFIRMA`, no
candidate M306K5 payload, and no updater metadata naming the lost package.
The only actual `CHGFIRMA.EXE` remains the already analyzed Satellite 1800
version 2.10 specimen with `EC30B100.BIN`; its payload identifies K7 hardware
and is not an M200 candidate.

Accordingly, this pass produces an archival fingerprint rather than an EC
algorithm. No executable was run and no hardware or QEMU source was touched.

## 1. Authentic M200 recovery procedure

### Primary source identity

The primary source is Toshiba, *Portege M200 Maintenance Manual*, file number
960-457, first edition November 2003. The local PDF SHA-256 is
`813f36841666fbfcb051f0903809f058d5ee2d67556aa3d8b174de5d9a6b6097`.
The same document is independently archived online.[^manual]

### Boot flow

| Stage | Proven behavior | Evidence |
|---|---|---|
| F001 | Check whether EC/KBC firmware rewrite is requested; branch to rewrite process if requested | manual table 2-3, p. 2-22 |
| F009 setup | Save key scan, set up `TASK_1ms_TSC`, display `EC/KBC UPDATE, BIOS UPDATE/DAMAGED` | manual table 2-3, pp. 2-22–2-23 |
| Media recognition | Reset FDC; try 1.44-MB 2HD and 720-KB 2DD parameters | manual table 2-3, p. 2-23 |
| File loading | Search FAT root, read directory sectors, locate `CHGBIOSA.EXE` or `CHGFIRMA.EXE`, read MZ header, execute; errors return to key input | manual table 2-3, p. 2-23 |
| User trigger | Insert floppy containing the updated program before power-on; tilde selects BIOS, Tab selects KBC/EC | Appendix F, p. F-1 |
| Completion | Beep and completion message; machine powers off automatically | Appendix F, p. F-1 |

The manual calls the appendix “BIOS/KBC/EC Update.” It does **not** say that
the technician types a DOS command. The machine's boot-block loader selects
and executes the root executable according to update request/key state.

The manual also does not explicitly use the term ASP. “ASP distribution” is
the project's provenance description. Broader Toshiba manuals call equivalent
media a “BIOS/EC/KBC rewriting disk for the computer” and warn that rewriting
should occur only when instructed by a “diagnostic disk release notice.”[^m10]
This is **STRONG INFERENCE** for a controlled service/diagnostic release
channel, not proof of the M200 disk's catalog name.

The exact extracted evidence and omissions are preserved in
`scratch/ec_re004/recovery_procedure.txt`.

## 2. What the M200 manual does not identify

No searched portion of manual 960-457 supplies:

- a Toshiba part, service, disk, or package number;
- an exact media title beyond “floppy diskette including updated program”;
- a volume label;
- a `CHGFIRMA.EXE` version;
- an EC image filename or wildcard;
- a model-selection or version command line;
- an EC firmware release date.

Those fields remain **UNKNOWN**. In particular, it would be unsound to turn
the related-model filename `EC30B100.BIN` into an M200 naming claim.

## 3. Packaging fingerprints

### Proven Toshiba convention from the related specimen

The offline Satellite 1800 package `b1830.exe` contains at archive root:

- `CHGFIRMA.EXE`, DOS MZ, CHGFIRM v2.10, 18,977 bytes, SHA-256
  `9d8a002949d5b8562f5c44bef5eb9d13fbaa823800cd6f426d5c358330e839ea`;
- `EC30B100.BIN`, 72,832 bytes, SHA-256
  `fb93de32c9f4791ab4053d2eff9d46d3416b44c351f7e8e542c68c8c78d72f19`;
- `CHGBIOSA.EXE` and `BIOFC72T.COM`.

Its executable states `CHGFIRMA [SW] [File]` and filename form
`ECxx????.BIN`, with `xx=[EC Type]` and `????=[free]`. This is **PROVEN** for
that 2001-era updater. It is only a packaging-search aid for the M200: the
companion payload labels itself `K7`, whereas the M200 board uses
M306K5F8LRP. No transfer-protocol or payload-layout identity is inferred.

### Candidate search patterns

Highest-value case-insensitive filename/content patterns are:

1. exact root name `CHGFIRMA.EXE` or `CHGFIRM*.EXE`;
2. sibling `EC??????.BIN`, especially the generic eight-character DOS form
   `ECxx????.BIN`;
3. a bootable 720-KB or 1.44-MB FAT image containing those root entries;
4. text `CHGFIRM (CHanGe FIRM Utility)`, `FIRM name struct err`, `EC Type`,
   `EC/KBC UPDATE`, `M306K5`, or `M16C/6K`;
5. service-release metadata containing M200, M205, `960-457`, “BIOS/EC/KBC
   rewriting disk,” or “diagnostic disk release notice.”

An exact M200 EC type code is **UNKNOWN**, so a guessed `ECxx` prefix is not
part of the fingerprint.

## 4. Exhaustive local archive search

### Scope and method

All files under `m200-data/` and the retained CHGFIRMA research set were
inventoried. Eighty-eight archive/image/executable/binary candidates were
container-listed with 7-Zip. ISO-contained M200 `.IMA` files were extracted
to a temporary directory and their FAT contents independently listed. Files
below 100 MiB were also searched as raw strings for `CHGFIRM`, `M306K5`,
M200/M205, EC/KBC rewrite terms, and DOS-style `EC*.BIN` names. No utility was
executed.

Supporting outputs include:

- `archive_candidates.txt`
- `archive_listing_hits.tsv`
- `nested_floppy_listings.txt`
- `nested_floppy_hits.txt`
- `binary_string_hits.txt`
- `m200_data_files.tsv`
- `local_archive_hits.tsv`

### M200 public package family

| Version | Date | Public package family | SHA-256 | EC result |
|---:|---:|---|---|---|
| 1.10 | 2004-01-29 | `3m200v11`, `1395D11`, `P0009V110`, `BIO0009T` | `77c0ce9534819513a3091ac1fecffbfd70305dbbf45248f57f689ce82be7bbb1` | absent |
| 1.20 | 2004-02-25 | `3m200v12`, `1395D12`, `P0009V120`, `BIO0009T` | `ee98e90218afb55a643b1fa84758c15ee41904277c596b81c419bb21697670ba` | absent |
| 1.30 | 2004-03-22 | `3m200v13`, `1395D13`, `P0009V130`, `BIO0009T` | `222870f328bf8e0394da1d3d30b462218cf031ae8ed0d10a707d50be13d7086f` | absent |
| 1.40 | 2004-07-16 | `3m200v14`, `1395D14`, `P0009V140`, `BIO0009T` | `0d74c503d49c3bcf714b432ad23359a5170a1070edfa4530aaabdd18385ae80c` | absent |
| 1.60 | 2004-07-27 | `3m200v16`, `1395D16`, `P0009V160`, `BIO0009T` | `408346fe8b0e5614ab8cca5d6721418802f8094a008b00b8d87751ea9129dac1` | absent |
| 1.70 | 2005-08-08 | `3m200v17`, `1395D17`, `P0009V170`, `BIO0009T` | `c3638d5d09dc5027c8cc9733715e41076adf56e39954606a2e328a7bf9c867b2` | absent |
| 1.80 | 2005-11-01 | `3m200v18`, `1395D18`, `P0009V180`, `BIO0009T` | `f1f56ac73a97aa24f2987e2b745812c0713a6204db4ca799bdb25cb23bb612db` | absent |

Every nested public floppy contains `CHGBIOSA.EXE` and `BIO0009T.COM`, not
`CHGFIRMA.EXE` or `EC*.BIN`. The missing 1.50 number in the held public series
does not establish that version 1.50 was an EC release; no `3m200v15`,
`1395D15`, or `P0009V150` record was found in the searches, so this is merely
a numbering gap.

The M200 factory T&D package `pom200t1-V1.5.exe` (SHA-256
`a466d750afe54536af227e059afd9b5bb63fd3908c0dc7cd7f396155fadbcd3f`)
also lacks the updater and payload. EC-RE-003 already proved that its
firmware-like HEX files target the Wacom LC87F1564, not M306K5.

### Downloaded archived driver bundle

The Internet Archive item “Toshiba Portege M200 XP Drivers and Utilities”
provides a 123,624,735-byte ZIP.[^ia-drivers] Its 33 members were statically
listed; no filename matched `CHGFIRM`, `M306K`, `EC*.BIN`, KBC firmware, or
rewrite terms. The locally downloaded ZIP hashes to
`cfa42b5f8f7f740bc4ae03b71bb28d020d7071f6ee31e85e5d9bf1b80e53057c`.

Several Internet Archive items expose full M200 operating-system recovery
ISOs in metadata, but none identifies itself as Toshiba ASP/service rewrite
media or exposes a matching top-level filename. Their multi-gigabyte ISO
interiors were not represented as having been exhaustively downloaded and
scanned in this pass. Consumer OS recovery media are lower-probability than
the separately documented service rewriting floppy.

### Result

The exact local M200/M205 distribution was not found. The only actual local
CHGFIRMA hit is the known Satellite 1800 K7 package. Therefore:

- authentic M200/M205 `CHGFIRMA.EXE`: **NO**;
- authentic M306K5 image: **NO**;
- recoverable F002 algorithm: **NO**;
- validated seven-byte programming ID: **NO**.

## 5. Package-identity and provenance correlation

The authentic public M200 BIOS packages form a coherent product family:
base package `3m200vXX.exe`, boot-disk wrapper `1395DXX.EXE`, Windows updater
`P0009VXXX.EXE`, and BIOS payload `BIO0009T.COM`. Contemporary archived file
listings independently show the same membership and dates for versions 1.30
and 1.80.[^v13][^v18]

This correlation is useful negatively: none of those package identifiers is
an EC rewrite identifier, and no adjacent public filename found in their
metadata points to an EC package. The service distribution may reuse
`CHGFIRMA.EXE`, but it should not be sought as a renamed `BIO0009T.COM` or
assumed to share the `P0009` identifier.

The strongest time window is **November 2003 through November 2005**:
November 2003 is the manual's first edition and January 2004 is the initial
public BIOS; 1.80 appeared in November 2005. This does not rule out a later
service release, but it centers the machine's active ASP support period.

## 6. Candidate validation outcome

No new candidate reached Pass F/H validation. Consequently no new bytes were
carved, and no M306K5 vector, updater target check, writer upload, checksum,
mailbox parser, or command 90/91/9E/9F handler can be attributed to M200.

The related K7 package is explicitly rejected as an M200 candidate because:

- its wrapper is identified with Satellite 1800;
- its payload metadata says `K7`;
- its 64-KiB system plus 4-KiB boot layout and uploaded-writer protocol are
  proven only for that package;
- the M200 part is M306K5F8LRP.

No arbitrary seven-byte constant was reclassified as an M306K5 serial ID.

## 7. Best exact archival fingerprint

For a future catalog, private archive, former ASP collection, or disk-image
search, request this exact artifact description:

> Toshiba Portégé M200/M205 BIOS/EC/KBC rewriting floppy, circa 2003–2005,
> associated with maintenance manual 960-457 and/or an M200 diagnostic-disk
> release notice; bootable 1.44-MB or possibly 720-KB FAT media; root file
> `CHGFIRMA.EXE`; likely companion DOS 8.3 file matching `EC*.BIN` (related
> Toshiba convention: `ECxx????.BIN`, exact M200 name unknown); target EC
> Mitsubishi/Renesas M306K5F8LRP / M16C/6K; invoked by holding Tab at power-on.

Additional useful search keys:

- literal text `CHGFIRM (CHanGe FIRM Utility)`;
- `EC/KBC UPDATE, BIOS UPDATE/DAMAGED`;
- “BIOS/EC/KBC rewriting disk for the computer”;
- “diagnostic disk release notice”;
- `Portege M200`, `Portege M205`, `M306K5F8LRP`, `960-457`;
- raw FAT root directory entry `CHGFIRMAEXE`;
- archive formats ZIP, LZH/LHA, CAB/self-extracting EXE, IMG/IMA/DSK, and ISO.

Do not narrow the search to files whose outer archive name contains M200:
service packages may be indexed by an internal release number. Conversely,
do not accept a generic Toshiba CHGFIRMA unless its metadata or code proves
M200/M205 and M306K5 targeting.

## 8. Evidence table

| Finding | Label | Basis |
|---|---|---|
| M200 boot recovery can execute root `CHGFIRMA.EXE` | **PROVEN** | authentic manual table 2-3 |
| Tab is the M200 EC/KBC update trigger | **PROVEN** | authentic manual Appendix F |
| F009 performs FAT/media/MZ loading | **PROVEN** | authentic manual table 2-3 |
| Exact media title or part number is documented | **UNKNOWN / not found** | absent from complete extracted manual |
| Artifact was service-channel rather than public BIOS distribution | **STRONG INFERENCE** | manual recovery context plus Toshiba “rewriting disk”/diagnostic-release convention |
| Exact M200 EC payload filename is `ECxx????.BIN` | **HYPOTHESIS** | related CHGFIRMA convention only |
| Public M200 BIOS versions 1.10–1.80 contain EC updater/payload | **DISPROVEN for held packages** | direct and nested archive inventories |
| Factory M200 T&D disk is the rewrite distribution | **DISPROVEN** | EC-RE-003 FAT and binary analysis |
| Related Satellite 1800 package is usable M200 firmware | **DISPROVEN** | K7 payload identity versus M306K5 board part |
| Authentic M200 updater or M306K5 payload recovered | **NO** | exhaustive local search and historical search results |

## 9. Mandatory verdicts

**A. Exact Toshiba EC/KBC rewrite media identity known — PARTIAL.** Its role,
target model family, boot trigger, root executable, loader behavior, probable
media geometry, and support-era window are known. Its catalog title, release
identifier, and exact bytes are not.

**B. Toshiba part/package number known — NO.** Manual 960-457 is a manual file
number, not proven to be the rewrite-disk part number.

**C. Expected M200/M205 EC payload filename known — PARTIAL.** An external
model-specific payload is strongly expected from Toshiba packaging and the
project's prior updater evidence. Only the broad `EC*.BIN` / related
`ECxx????.BIN` search form is known; the exact M200 filename and EC type are
unknown.

**D. Authentic M200/M205 CHGFIRMA found — NO.**

**E. Authentic M306K5 firmware found — NO.**

**F. F002 algorithm recovered — NO.**

**G. Seven-byte ID recovered — NO.**

**H. Best archival fingerprint —** the block in section 7, centered on a
2003–2005 M200/M205 BIOS/EC/KBC rewriting floppy tied to manual 960-457 or a
diagnostic-disk release notice, with root `CHGFIRMA.EXE`, a model-specific
`EC*.BIN`, and M306K5F8LRP target evidence.

**I. Single safest next action — submit that exact fingerprint to a former
Toshiba ASP/service-media archive holder and request a read-only disk image or
original archive plus provenance metadata.** The returned bytes should then
be hashed and container-listed offline before any executable analysis. This
is one acquisition action; it neither executes an updater nor touches the
physical M200.

## 10. Safety and repository impact

No service executable or firmware utility was executed. No physical-machine
I/O, flash operation, mode-pin change, or programming attempt occurred. No
QEMU source was modified. No commit or push was made. Existing dirty source
and research material was preserved.

[^manual]: Toshiba, *Portege M200 Maintenance Manual (960-457)*, archived copy: https://macdat.net/files/pdf/toshiba/service_manual/portege_m200.pdf
[^m10]: Toshiba, *Satellite Pro M10 Maintenance Manual (960-431)*, Appendix H reproduction: https://toshiba.manymanuals.com/computers/m10/user-manual-25182/457
[^ia-drivers]: Internet Archive, *Toshiba Portege M200 XP Drivers and Utilities*: https://archive.org/details/toshiba-portege-m200-xp-tablet-05
[^v13]: Archived Toshiba M200 BIOS 1.30 package listing: https://www.driverguide.com/driver/detail.php?driverid=153362
[^v18]: Archived Toshiba M200 BIOS 1.80 package listing: https://www.driverguide.com/driver/detail.php?driverid=1720457
