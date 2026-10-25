# M200 Diagnostic Environment — Forensic Reverse Engineering Report

> **IMPORTANT**: This report is based on **instruction-level disassembly evidence** from 35+ DOS binaries.
> Previous preliminary analyses contained multiple incorrect interpretations that are
> explicitly corrected below. Every finding is evidence-classified.

## 1. Artifact Provenance

| Artifact | MD5 | Size |
|----------|-----|------|
| `pom200t1-V1.5.exe` (SFX) | `48b5c95febf82cf5de78189b064ea940` | 720,579 |
| `pom200t150.IMA` (floppy) | `f6f42279a4652a9c5954639acf1888e2` | 1,474,560 |
| BIOS reference | `TOSHIBA_M200_V180_FLASH512K.BIN` | 524,288 |

**Source:** Official Toshiba factory maintenance diskette, T&D v1.50, dated 2004.

---

## 2. Corrections to Previous Analyses

> **CAUTION**: The preliminary analyses repeatedly produced incorrect conclusions from filenames and strings.
> The following are now **DISPROVEN** by instruction-level evidence.

| Previous claim | Actual finding | Status |
|----------------|----------------|--------|
| BOOT.COM performs `OUT 0xCF9, 0x06` | SMI via `OUT 0xB2, 0xFE`, warm boot flag, `JMP FFFF:0000` | **DISPROVEN** |
| EHSS.COM directly writes CMOS 0x72/0x73 and EC | Delegates entirely to BIOS via `INT 15h AX=44FEh/44FFh` — zero direct I/O | **DISPROVEN** |
| CHGMC.COM writes to flash ROM and PCI registers | Delegates to BIOS SMI via `INT 15h` — no CF8/CFC, no FWH access | **DISPROVEN** |
| KBDC.COM accesses EC via ports 0x62/0x66 | Only reads keyboard scan codes from 0x60/0x64 — never writes commands | **DISPROVEN** |
| FLASH.EXE is a BIOS/EC flasher | Wacom digitizer flasher via serial COM port (default 0x338) | **DISPROVEN** |
| V4S_*.HEX are EC firmware | Wacom ISD-V4 digitizer firmware for Sanyo LC87F1564 | **DISPROVEN** |
| MCUD.COM is "Micro Controller Update" for EC | "Processor Microcode Update" — uses CPUID + RDMSR + INT 15h | **DISPROVEN** |
| Tools directly manipulate FWH at 0xFFF80000 | All flash writes go through Toshiba INT 15h SMI interface | **DISPROVEN** |
| DS.INI is a network/DataSave config | Yamaha DS-XG audio test configuration used by TESTFF.EXE | **DISPROVEN** |
| PCIRQ reads ELCR / direct ICH4-M registers | Uses PCI BIOS INT 1Ah exclusively | Partially **DISPROVEN** |

---

## 3. Boot & Factory Workflow

*(Full details: scratch/diag_re/workflow.md, scratch/diag_re/workflow.dot)*

```
IO.SYS -> CONFIG.SYS menu:
  |-- CE01 "Repair Main (T&D)" -> AUTOEXEC.BAT -> MAIN.BAT -> enfir -> testff
  +-- CE0F "Repair initial config set" -> AUTOEXEC.BAT -> SUB01.BAT menu:
       |-- 1 -> 1.BAT (Factory provisioning)
       |-- 8 -> 8.BAT (System config display via syscnf)
       +-- 9 -> 9.BAT (MAC/DMI verification)
```

### Factory Provisioning Flow (1.BAT)

1. **CPU Table Check:** `cpt_chk` -> if errorlevel 9, needs table update
2. **CPU Detection:** `cpt_sel` -> Banias(1), Dothan(2), Celeron(3), Other(4)
3. **Table Injection:** `chgmc /cf cpinft_B.bin` or `cpinft_D.bin` -> reboot
4. **Microcode Update:** `mcud` -> if update needed, reboot
5. **Digitizer Firmware:** `unitchk` -> old/new -> `flash v4s_004A.hex` or `v4s_0055.hex`
6. **DMI/UUID:** `dmiw` -> `setuuid` -> `dmir` (verify)
7. **Display config:** `hwcinf`
8. **Return to menu:** `sub01`

### DVD Region (2.BAT)
Reads market code from DMI via `DMIRPC2`, writes RPC2 region to DVD drive via ATAPI commands.

---

## 4. Global I/O / Hardware Access Map

### Key Discovery: The Toshiba INT 15h SMI Interface

> **IMPORTANT**: Most diagnostic tools do NOT access hardware directly. They delegate to the BIOS
> through a proprietary SMI interface: **INT 15h, AX=44FEh** (check support) and
> **AX=44FFh** (execute). The BIOS SMI handler performs actual flash writes, CMOS
> modifications, and EC communication in System Management Mode.

**Programs using INT 15h AX=44xxh SMI:** EHSS, HWSC, CHGMC, MCUD, CPT_CHK, DMIW, SETUUID, HWCINF

### Direct Hardware Access (PROVEN)

| Binary | Port | Dir | Value | What | Evidence |
|--------|------|-----|-------|------|----------|
| BOOT.COM | 0xB2 | OUT | 0xFE | SMI trigger for reboot | PROVEN: offset 0x16A |
| BOOT.COM | 0040:0072 | WRITE | 0x1234 | Warm boot flag | PROVEN: offset 0x1D6 |
| BOOT.COM | FFFF:0000 | JMP | — | BIOS reset vector | PROVEN: offset 0x1E0 |
| PCIRQ.COM | INT 1Ah | CALL | AX=B10Bh | PCI BIOS Write Config Byte | PROVEN |
| PCIRQ.COM | INT 1Ah | CALL | AX=B108h | PCI BIOS Read Config Byte | PROVEN |
| ENFIR.COM | 0x2E/0x2F | OUT | Config | SMSC Super I/O LPC47N217 | PROVEN |
| MCUD.COM | CPUID | — | EAX=1 | CPU signature | PROVEN: offset 0x123 |
| MCUD.COM | RDMSR | — | ECX=0x8B | IA32_BIOS_SIGN_ID | PROVEN: offset 0x3DC |
| MCUD.COM | RDMSR | — | ECX=0x17 | IA32_PLATFORM_ID | PROVEN: offset 0x12C |
| MCUD.COM | WRMSR | — | ECX=0x8B | Clear signature buffer | PROVEN: offset 0x3CC |
| SYSCNF.COM | 0xCF8/0xCFC | IN/OUT | — | PCI config enumeration | PROVEN |
| SYSCNF.COM | 0x62/0x66 | IN/OUT | — | ACPI EC queries | PROVEN |
| SYSCNF.COM | 0x60/0x64 | IN | — | KBC status/data | PROVEN |
| SYSCNF.COM | 0x70/0x71 | IN/OUT | — | CMOS reads | PROVEN |
| ICHXLAN.COM | 0xCF8/0xCFC | IN/OUT | — | PCI config for NIC | PROVEN |
| ICHXLAN.COM | INT 1Ah | CALL | AX=B102h | PCI Find Device 8086:1229 | PROVEN |
| FLASH.EXE | 0x338+ | IN/OUT | — | Serial UART (Wacom) | PROVEN |
| FLASH.EXE | 0x60/0x62 | IN/OUT | — | EC handshake for digitizer | PROVEN |
| CRTCS.COM | 0x3D4/0x3D5 | IN/OUT | — | VGA CRTC registers | PROVEN |
| CRTCS.COM | 0xCF8/0xCFC | IN/OUT | — | PCI config for GPU | PROVEN |
| KBDC.COM | 0x60/0x64 | IN | — | Keyboard scan code read | PROVEN |
| KBDC.COM | 0x61 | OUT | 0x03 | PC speaker beep | PROVEN |

---

## 5. KBDC Protocol

**KBDC.COM is a passive keyboard scan code reader — PROVEN**

| Port | Direction | Value | Meaning | Offset | Confidence |
|------|-----------|-------|---------|--------|------------|
| 0x64 | IN | — | Read KBC status (bit 0=OBF, bit 5=MouseOBF) | 0xBD2 | PROVEN |
| 0x60 | IN | — | Read keyboard scan code | 0xBE4 | PROVEN |
| 0x61 | IN/OUT | 0x03 | PC speaker beep for error | 0x14D9 | PROVEN |

KBDC.COM **never writes any commands** to port 0x60 or 0x64. It only:
1. Polls bit 0 (OBF) and bit 5 (Mouse OBF) of status port 0x64
2. Discards mouse bytes (when bit 5 set)
3. Reads keyboard scan codes from port 0x60
4. Beeps via port 0x61

---

## 6. F002 Correlation

### BIOS F002 Security Controller Protocol (PROVEN from BIOS disassembly)

The BIOS POST F002 checkpoint uses the **ACPI Embedded Controller** interface:

```asm
; Write command to EC (port 0x66) — BIOS offset 0xFF32E
000FF332  E466          in al,0x66       ; Read EC Status
000FF334  A802          test al,0x2      ; Check IBF (bit 1)
000FF336  740C          jz 0xf344        ; Wait for IBF=0
000FF348  E666          out 0x66,al      ; Write EC Command

; Write data to EC (port 0x62) — BIOS offset 0xFF34D
000FF351  E466          in al,0x66       ; Read EC Status
000FF353  A802          test al,0x2      ; Check IBF (bit 1)
000FF367  E662          out 0x62,al      ; Write EC Data

; Security init sequence:
000FC1B5  B09E          mov al,0x9e      ; CMD: Start collecting
000FC1BF  B011          mov al,0x11      ; DATA: byte 1
000FC1C9  B010          mov al,0x10      ; DATA: byte 2
; Then CMD 0x9F to process, reads 2-byte response from port 0x62
```

### KBDC.COM vs F002: **NO — PROVEN**

| Aspect | KBDC.COM | BIOS F002 |
|--------|----------|-----------|
| Ports | 0x60/0x64 | 0x62/0x66 |
| Direction | Read-only | Read + Write |
| Protocol | Passive scan code reader | Multi-byte command/response |
| Commands sent | None | 0x9E, 0x9F, data bytes |
| Status bits | Bit 0 (OBF) for read | Bit 1 (IBF) for write |

**F002 controller/interface identified? NO — the diagnostic floppy contains no program that exercises the security controller protocol.**

---

## 7. 855PM / Banias / Dothan Findings

*(Full details: scratch/diag_re/pass_e_855pm_cpu.md)*

### CPT_CHK.COM — PROVEN
- Uses INT 15h AX=44F1h/44F2h/44F3h (Toshiba SMI)
- Returns ERRORLEVEL 0 (OK) or 9 (needs table update)
- Does **NOT** use CPUID or RDMSR

### CPT_SEL.COM — PROVEN
- Reads CPU signature from **PnP BIOS** structure (searches F000:0000 for `$PnP`, calls function 0x51)
- Does **NOT** execute CPUID directly
- ERRORLEVEL mapping: 1=Banias(0x0695), 2=Dothan(0x06D6), 3=Celeron, 4/5=Other

### CHGMC.COM — PROVEN
- "CHanGe Micro Code" — loads CPINFT payload and delegates to BIOS via INT 15h SMI
- **Zero** direct PCI config, FWH, or MSR access
- The actual flash write occurs in SMM

### CPINFT Files — PROVEN structure
- **CPINFT_B.BIN** (4608 bytes): 9 records x 512 bytes, CPU signature 0x0695 (Banias)
- **CPINFT_D.BIN** (11264 bytes): 22 records x 512 bytes, CPU signature 0x06D6 (Dothan)
- Records contain Toshiba-proprietary CPU configuration tables (likely FID/VID, thermal data)

### BANIDOTH.PDB — PROVEN format
- Standard concatenated Intel Microcode Update file (6 x 2048 bytes = 12288)
- Records: Banias 0x0695 (rev 0x45, 0x05), Dothan 0x06D1 (rev 0x07), 0x06D2 (rev 0x10), 0x06D6 (rev 0x14)

### MCUD.COM — PROVEN
- "Processor Microcode Update" — uses CPUID (EAX=1) and RDMSR (MSR 0x8B, 0x17)
- Clears MSR 0x8B via WRMSR before CPUID (Intel standard procedure)
- Does **NOT** trigger MSR 0x79 (IA32_BIOS_UPDT_TRIG) — delegates flash write to BIOS via INT 15h
- **Previous claim "Micro Controller Update" is DISPROVEN**

### 855PM Register Tables — HYPOTHESIS
The CPINFT files likely contain i855PM-specific register settings, but this cannot be confirmed without the INT 15h SMI handler disassembly. The tools themselves never touch PCI 00:00.0.

---

## 8. ICH4-M Findings

### PCIRQ.COM — PROVEN
- Method: PCI BIOS INT 1Ah (AX=B108h read, AX=B10Bh write)
- **Does NOT** read PIRQx registers at BDF 00:1F.0 — instead writes to BDF 02:0D.0/02:0D.6
- Target device is on **Bus 2** (likely CardBus bridge or audio controller)
- **Previous claim** that it reads PIRQA-H at offsets 0x60-0x6B is **NOT CONFIRMED** from this binary

### BOOT.COM — PROVEN
- Resets via: SMI (`OUT 0xB2, 0xFE`), warm boot flag (0x1234 at 0040:0072), `JMP FFFF:0000`
- **Port 0xCF9 is NEVER accessed** — ICH4-M reset control register claim is DISPROVEN

### ICHXLAN.COM — PROVEN
- Target: Intel 8255x PRO/100 family (PCI Find Device: vendor 0x8086, device 0x1229)
- Uses both PCI BIOS INT 1Ah and direct CF8/CFC config access
- Accesses NIC CSR registers via BAR0 memory-mapped I/O
- **NOT** an "ICH4-M integrated LAN" test — it targets discrete Intel 8255x NIC

---

## 9. FWH Findings

> **WARNING**: No diagnostic tool directly accesses the FWH at memory-mapped addresses.
> All claims about FWH addresses (0xFFF80000, 0xFFBC0000 lock registers) are **UNPROVEN**
> from this diagnostic disk.

- **CHGMC, DMIW, SETUUID, MCUD**: All use Toshiba INT 15h SMI calls (AX=44FEh/44FFh)
- The actual FWH manipulation (lock/unlock, erase, program) occurs entirely in **SMM** by the BIOS firmware
- **Flash chip identification** (Intel E82802AB) is not performed by any diagnostic tool

### FWH Access via SMI — PROVEN
```asm
; CHGMC.COM at offset 0x5D1
000005D1  B8FE44        mov ax,0x44fe    ; Toshiba BIOS check support
000005D4  BB5700        mov bx,0x57      ; Function selector
000005E8  CD15          int 0x15         ; Trigger SMI
```

---

## 10. CPU Banias/Dothan Findings

### MCUD.TXT Values — PROVEN context
| Field | Value | Source | Classification |
|-------|-------|--------|----------------|
| Processor `000006D6h` | Dothan Family 6 Model 13 Step 6 | RDMSR MSR 0x8B after CPUID EAX=1 | **PROVEN** runtime measurement |
| Revision `00000017h` | Microcode patch level | RDMSR MSR 0x8B high dword | **PROVEN** runtime measurement |
| Date `02192004h` | Feb 19 2004 | Read from flash microcode header | **PROVEN** stored data |

### BANIDOTH.PDB Microcode Records — PROVEN
| Offset | CPU Signature | Revision | Platform Flags |
|--------|---------------|----------|----------------|
| 0x0000 | 0x0695 (Banias) | 0x45 | 0x80 |
| 0x0800 | 0x0695 (Banias) | 0x05 | 0x20 |
| 0x1000 | 0x0695 (Banias) | 0x45 | 0x80 |
| 0x1800 | 0x06D1 (Dothan A0) | 0x07 | 0x20 |
| 0x2000 | 0x06D2 (Dothan A1) | 0x10 | 0x20 |
| 0x2800 | 0x06D6 (Dothan B1) | 0x14 | 0x02 |

---

## 11. HWC/SYSCNF Hardware Map

### SYSCNF.COM — Mixed data sources (PROVEN)
| Data source | Method | Evidence |
|-------------|--------|----------|
| PCI devices | Direct CF8/CFC config reads | PROVEN: `IN EAX,DX` with DX=0xCFC |
| EC data | ACPI EC ports 0x62/0x66 | PROVEN: battery, thermal queries |
| KBC status | Standard ports 0x60/0x64 | PROVEN |
| CMOS values | Ports 0x70/0x71 | PROVEN |
| Toshiba HCI | INT 15h BX=70h | PROVEN |

### HWCINF.COM — Primarily BIOS-returned (PROVEN)
| Data source | Method | Evidence |
|-------------|--------|----------|
| Model/UUID/Serial | INT 15h AX=44FEh (Toshiba HCI) | PROVEN |
| CMOS config | Ports 0x70/0x71 | PROVEN |
| Display | INT 10h | PROVEN |

---

## 12. FLASH.EXE / V4S / Digitizer Identification

### FLASH.EXE — PROVEN: Wacom Digitizer Flasher
- Identified by string: `WACOM F/W Update Program Ver 1.00`
- Default COM port: `0x338` (non-standard — internal Wacom serial)
- Default IRQ: 4
- Transport: Serial UART polling
- Also accesses EC ports 0x60/0x62 for digitizer mode handshake

### V4S HEX Files — PROVEN: Sanyo LC87F1564
- Header comment: `[Chip name] LC87F1564`
- File name reference: `isdv4.x87` (ISD-V4 = Wacom Intuos Serial Digitizer V4)
- V4S_004A.HEX: 4104 records (old digitizer unit)
- V4S_0055.HEX: 4097 records (new digitizer unit)

**Previous claim that these are EC firmware is DISPROVEN.**

---

## 13. Display Findings (CRTCS.COM)

### VBE Usage — PROVEN
- Sets VBE mode 0x105 (1024x768x8) via `INT 10h AX=4F02h, BX=0x0105`
- Note: Independent BIOS RE found mode 0x103 (800x600) / BX=0x4103 — **different mode**
- Uses standard VGA DAC palette programming (0x3C8/0x3C9)
- Unlocks CRTC registers via 0x3D4/0x3D5 register 0x11
- Uses VGA Attribute Controller (0x3C0) with palette address source

### PCI Access — PROVEN
- Accesses PCI config at Bus 0, Dev 8, Func 0, offset 0x74
- ORs register with 0x08000000 (bit 27) and writes back
- **No NV34M-specific MMIO register access detected** — uses only VBE/VGA standard interfaces

---

## 14. Other Platform Findings

### Audio (DS-XG/AC'97) — STRONG INFERENCE
- `DS.INI` = Yamaha DS-XG audio test config consumed by TESTFF.EXE
- FM/SB port values (0x388, 0x220) are **test probe lists**, not fixed M200 config
- `INTAIRQ=11` is M200-specific PCI audio interrupt routing
- `GPIO0=LOW` appears in MAIN2.ATN secondary codec test — M200-specific amplifier unmute

### IrDA — PROVEN from disassembly
- ENFIR.COM hard-codes: FIR base=0x0100, IRQ=3, DMA=1
- Configures SMSC Super I/O via ports 0x2E/0x2F (config mode 0x55/0xAA)
- Selects Logical Device 5 (IrDA/FIR)
- These are **M200-specific hardware values**, not defaults

### LAN — PROVEN
- ICHXLAN.COM targets Intel 8255x PRO/100 (vendor 0x8086, device 0x1229)
- I558DRV.COM is Intel 82558/82559/82550 DOS driver
- Uses PCI BIOS + direct CF8/CFC for NIC configuration
- Manages EEPROM for MAC address via NIC CSR registers

---

## 15. Safety Classification

### SAFE READ-ONLY
`DMIR`, `CHKDMIW`, `HWCINF`, `SYSCNF`, `CPT_CHK`, `CPT_SEL`, `unitchk`, `RDHDDID`, `FDDMP`, `CLSV`

### MIXED READ/WRITE (volatile registers only)
`KBDC`, `CRTCS`, `EXPC`, `FIREV`, `USB01`, `USB23`, `SNDBASE2`, `SNDBASE3`, `I558DRV`, `EXLP558`, `IPSI`, `IPSP`, `IPSSW`, `IPS4`, `HDDID`

### PROBABLY READ-ONLY
`TESTFF` (interactive T&D — mainly read-only tests)

### STATE-CHANGING (persistent modifications)
| Tool | What it changes |
|------|----------------|
| `BOOT.COM` | System reset (SMI + JMP FFFF:0) |
| `CHGMC.COM` | CPU tables in BIOS flash (via SMI) |
| `MCUD.COM` | Microcode in BIOS flash (via SMI) |
| `FLASH.EXE` | Wacom digitizer firmware (via serial) |
| `DMIW.COM` | DMI data in flash (via SMI) |
| `SETUUID.COM` | UUID in flash |
| `EHSS.COM` | SpeedStep flag in NVRAM (via SMI) |
| `HWSC.COM` | Hardware scan flag in NVRAM (via SMI) |
| `ICHXLAN.COM` | LAN EEPROM (MAC address) |
| `ENFIR.COM` | Super I/O configuration registers |
| `DFWC_NEW/OLD` | Digitizer flash check/prep |
| `DMIRPC2.COM` | DVD region via ATAPI |
| `FDFMT/FDCPY` | Floppy disk formatting |

---

## 16. QEMU Relevance

### High Relevance for Emulation
| Finding | QEMU Impact |
|---------|-------------|
| F002 security controller uses ports 0x62/0x66 with commands 0x9E/0x9F | Confirms existing QEMU implementation is correct |
| No diagnostic tool exercises F002 protocol | Cannot use diagnostics to test security controller emulation |
| Toshiba INT 15h AX=44FEh/44FFh SMI interface | Must emulate for factory tools to work in QEMU |
| SYSCNF does direct PCI enumeration via CF8/CFC | Validates PCI device topology matters |
| SYSCNF queries ACPI EC via 0x62/0x66 | EC emulation must respond to standard queries |
| ENFIR configures Super I/O at 0x2E/0x2F | Super I/O emulation needed for IrDA |
| ICHXLAN targets Intel 8255x (device 0x1229) | NIC device ID must be 0x1229 for diagnostics |
| CRTCS uses VBE mode 0x105 (1024x768) | VBE mode support needed beyond 0x103 |
| MCUD uses CPUID + MSR 0x8B/0x17 | CPU must report correct signature and microcode revision |
| TESTFF exercises PIC/PIT/DMA/RTC | Comprehensive validation suite for peripheral emulation |

### Not Useful for Emulation
- KBDC.COM — too simple (passive scan code reader)
- FLASH.EXE/V4S — Wacom digitizer, not relevant to core chipset
- CPINFT binary tables — opaque without SMI handler disassembly

---

## 17. Unknowns

1. **INT 15h AX=44FEh/44FFh handler** — the SMI handler that processes flash writes is in the BIOS ROM, not analyzed here
2. **EXPC.COM target device** — accesses Bus 2 devices, possibly Toshiba YEBISU-SS or standard CardBus
3. **PCIRQ.COM BDF 02:0D.0** — what device is at this BDF on the M200?
4. **HOSEI.COM** — "calibration correction" tool, insufficiently analyzed
5. **BANIDOTH.PDB detailed record contents** — confirmed as Intel microcode format but payloads not decoded
6. **CPINFT record structure beyond signature** — the 512-byte records likely contain FID/VID tables but field mapping requires SMI handler analysis

---

## 18. Recommended Future Physical Measurements

1. **Capture INT 15h AX=44FEh/44FFh SMI handler** from the BIOS ROM
2. **Run SYSCNF on physical M200** and capture output — provides authoritative PCI device list
3. **Run PCIRQ on physical M200** and capture output — reveals actual PIRQ routing table
4. **Trace ENFIR.COM Super I/O writes** on physical M200
5. **Dump EC RAM** via the INT 15h interface or direct port access
6. **Capture PCI config space** for BDF 02:0D.0 and 02:0D.6
7. **Test TESTFF.EXE** in QEMU when emulation is sufficiently advanced

---

## Final Summary Table

| Finding | Status | Primary Evidence | QEMU Relevance |
|---------|--------|------------------|----------------|
| F002 controller | **Ports 0x62/0x66, cmds 0x9E/0x9F — PROVEN** | BIOS disasm at 0xFF32E-0xFF367, 0xFC1B5 | Confirms existing implementation |
| KBDC interface | **Passive 0x60/0x64 reader — PROVEN** | KBDC.asm offset 0xBD2-0xBE4 | No F002 correlation (NO) |
| 855PM tables | **CPINFT_B (9x512B Banias), CPINFT_D (22x512B Dothan) — PROVEN structure** | Hex dump, exact file sizes | Opaque without SMI handler |
| ICH4-M PIRQ | **PCI BIOS INT 1Ah, targets Bus 2 — PROVEN** | PCIRQ.asm INT 1Ah calls | Partial — need LPC bridge data |
| CF9 reset | **DISPROVEN — uses SMI 0xB2 + JMP FFFF:0** | BOOT.asm offset 0x165-0x1E0 | No CF9 handling needed for boot tool |
| E82802AB mapping | **UNPROVEN — no tool accesses FWH directly** | All flash via INT 15h SMI | SMI handler analysis needed |
| Banias/Dothan config | **CPUID 0x0695/0x06D6, BANIDOTH.PDB verified — PROVEN** | MCUD.asm CPUID+RDMSR, hex dump | CPU signature + microcode emulation |
| V4S target MCU | **Sanyo LC87F1564 (Wacom ISD-V4) — PROVEN** | HEX file header comment | Not core chipset — low priority |
| Audio config | **INTAIRQ=11, GPIO0=LOW — STRONG INFERENCE for M200** | DS.INI, MAIN2.ATN consumed by TESTFF | Audio IRQ routing |
| IrDA config | **FIR=0x100, IRQ=3, DMA=1 — PROVEN M200-specific** | ENFIR.asm Super I/O programming | Super I/O emulation |
| LAN target | **Intel 8255x PRO/100, device 0x1229 — PROVEN** | ICHXLAN PCI Find Device call | NIC device ID for PCI topology |
| Toshiba INT 15h SMI | **AX=44FEh/44FFh is the universal flash/config interface — PROVEN** | Multiple binaries | Critical for factory tool support |
