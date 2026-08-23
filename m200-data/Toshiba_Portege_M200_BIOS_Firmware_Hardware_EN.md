# Toshiba Portégé M200 — BIOS / Firmware / Hardware Reference

Source: *Toshiba Personal Computer Portégé M200 Maintenance Manual*
(File Number 960-457, First edition November 2003, © Toshiba Corporation, 322 pages)

This document was compiled by scanning the entire uploaded PDF and
extracting every section directly relevant to **BIOS, firmware, and
hardware** — for the purpose of booting the BIOS in QEMU, modding it,
testing the mod in QEMU, and eventually potentially flashing it to a
real device. The physical disassembly/reassembly steps (Chapter 4,
mostly photo-based) are summarized for context; tables not directly
tied to firmware (e.g. raw RAM signal pin maps) have been condensed —
I can re-extract any of them in full if you need them.

---

## 1. System Overview

**Platform:** Toshiba Portégé M200 (Tablet PC), 2003

| Component | Detail |
|---|---|
| CPU | Mobile Pentium-M "Banias" 1.4/1.5/1.6/1.7 GHz, 400 MHz FSB, 1.48/0.96V core voltage, SpeedStep III support, built-in NDP (FPU) |
| L1 Cache | 32KB Data + 32KB Instruction, Write-Back |
| L2 Cache | 1MB (in CPU) |
| North Bridge | Intel 855PM "Odem+" (B-step) — Banias/Dothan FSB support, DDR333/DDR266 DRAM controller (2GB max), AGP 2.0 x4, Hub Link interface, 593-ball FC-BGA |
| South Bridge | Intel ICH4-M — Hub Link, PCI Rev2.2 (6 REQ/GNT pairs), UltraATA 100/66/33, USB1.1/2.0 (6 ports, EHCI), I/O APIC (ACPI 1.06), SMBus2.0, FWH interface (BIOS), LPC interface (EC/KBC, Super I/O), IRQ/Serial Interrupt Controller, Power Management Controller, Deeper Sleep (C4), AC'97 2.2, internal RTC, internal LAN (WfM2.0), 421-ball BGA |
| PC Card Gate Array | YEBISU-SS — PCI Rev2.2, CardBus/PC Card (Yenta2 v2.2), SD memory card (SDHC v1.2), SD I/O (v1.0), SmartMedia (SMHC v01/SMIL1.0), SIO/UART (MS Debug Port Spec v1.0), docking interface |
| GPU | nVIDIA MAP34-232, 32MB VRAM (4M×32×2 DDR 200MHz), AGP R2.0 x4, LVDS 2ch LCD interface |
| Memory | 2× DDR SO-DIMM (200-pin, 3.3V), DDR333(standard)/DDR266, PC2700, 6ns access time, max 2GB (128/256/512MB/1GB modules) |
| HDD | 2.5" x 9.5mm, 40/60/80GB (Toshiba/Hitachi GST models), UltraATA100 interface |
| LAN Controller | ICH4-M + Kinnereth (10/100Base-T) |
| Modem | MDC (Modem Daughter Card), ITU-T V.90/V.92 |
| Wireless LAN | Mini-PCI Type III, Atheros 802.11b/g |
| Bluetooth | v1.1, over USB host interface |

---

## 2. BIOS ROM (Flash Memory) — Detailed Map

> One of the most critical sections for QEMU emulation.

- **Chip:** Intel FWH (Firmware Hub) **E82802AB8** — IC3000 (on the system board)
- **Capacity:** 4 Mbit (256K × 16-bit)
- **Operating voltage:** 5.0V
- **Access time:** 120 ns or 90 ns
- **Data transfer:** 8-bit
- **Interface:** connects to ICH4-M via the FWH Interface (listed as "FWH Interface (BIOS)" inside the South Bridge)

### 4 Mbit (512KB) BIOS ROM content breakdown:

| Region | Size | Content |
|---|---|---|
| Logo | 64 KB | Boot logo |
| Setup & Checksum | 64 KB | Setup data + checksum |
| System BIOS | 128 KB | Main system BIOS code |
| VGA-BIOS | 64 KB | VGA BIOS |
| ACPI | 64 KB | ACPI tables |
| PnP | 8 KB | Plug and Play |
| Password Security | 8 KB | Password security |
| Booting | 16 KB | Boot block |
| LAN | 64 KB | Built-in LAN BIOS/option ROM |
| Reserved | 32 KB | — |

**Total:** 512 KB (64+64+128+64+64+8+8+16+64+32 = 512KB) ✓

---

## 3. Other Main System Chips (Firmware/Controller-Carrying)

| Chip | Role | Location (Board Layout) |
|---|---|---|
| **EC/KBC** — Mitsubishi LPC microcontroller **M306K5F8LRP** ×1 | Embedded Controller / Keyboard Controller (keyboard, power management, system events) | **IC3200** |
| **BIOS FWH** — Intel **E82802AB8** ×1 | System BIOS (above) | **IC3000** |
| **PSC** — Toshiba **TMP87PM48U** ×1 | Power Supply Controller (power controller, battery charging, DC-IN detection) | IC8972 |
| Temperature sensor | AND **ADM1032** ×1 | — |
| Temperature switch (for forcible CPU stop) | Maxim **MAX6501UKP** ×1 | — |
| Super I/O | SMSC **LPC47N217-JN** ×1 | — |
| Sound codec | AND **AD1981B** ×1 (AC'97) | — |
| Clock Generator | ICS **ICS950812CGT** ×1 | — |
| LAN PHY | Kinnereth (paired with ICH4-M) | IC4010 |

**Note:** the EC/KBC functions both as the keyboard controller and as
the system management controller (connected to ICH4-M over the LPC
bus). During BIOS updates, "BIOS" and "KBC/EC" can each be flashed
separately (see Section 7).

---

## 4. System Board Layout (Board Layout — Apx. B)

### 4.1 System Board Front View
| Mark | Reference | Part |
|---|---|---|
| (A) | IC8972 | PSC |
| (B) | PJ6005 | Speaker connector |
| (C) | **PJ2000** | **Debug Port connector** |
| (D) | PJ3240 | Touch pad connector |
| (E) | PJ2110 | PCMCIA I/F connector |
| (F) | PJ3230 | Keyboard connector |
| (G) | PJ3260 | LED board I/F connector |
| (H) | **IC3000** | **BIOS (FWH)** |
| (I) | **IC3200** | **EC/KBC** |

### 4.2 System Board Back View
DC-IN (PJ8800), USB0/1 (PJ4600/PJ4601), LCD LVDS (PJ5205)/PS-FLINV
(PJ5206), RGB (PJ5620), digitizer SW (PJ3540), docking (PJ2300), CPU
socket (IS1050), RJ11 relay (PJ3021), RJ11/RJ45 (PJ4100), panel
rotation sensor (PJ3280), Kinnereth LAN PHY (IC4010), CPU fan (PJ8400),
memory connectors (IS1440/IS1420), YEBISU-SS (IC2000), SD card I/F
(PJ2130), sound jacks (PJ6003/PJ6004), MDC (PJ3020), internal
microphone (PJ6000), **North Bridge (IC1200)**, battery connector
(PJ8810), RTC battery (PJ8490), **VGA (IC5000)**, HDD I/F (PJ1800),
Bluetooth (PJ4410), **South Bridge (IC1600)**, second fan/VGA
(PJ8425).

### 4.3 Other boards
- **Power Switch Board:** S3290 (power switch), PJ3290 (system board I/F)
- **LED Board:** PJ3261 (system board I/F)
- **Application Switch Board:** S3293 (4-direction + push, Cross
  Function), S3292 (Rotation+ESC), S3291 (Ctrl+Alt+Del / Windows
  Security tablet button), PJ3291 (system board I/F)

---

## 5. Critical Connectors — Pin Assignment (Apx. C)

Appendix C contains the full pin map for 30 connectors in total
(memory, LCD, CRT, USB, HDD, debug port, PC Card, keyboard, PAD, panel
sensor, LAN, Mini PCI, Bluetooth, DC-IN, battery, RTC battery, FAN,
speaker, microphone, headphone, SD, MDC, docking, digitizer SW,
board-to-board connectors). The one most critical for firmware/debug
work:

### PJ2000 — Debug Port connector (4-pin)
| Pin | Signal | I/O |
|---|---|---|
| 1 | B3V | — |
| 2 | DBGRX-B3P | O |
| 3 | DBGTX-B3P | I |
| 4 | GND | — |

This port is compliant with the MS Debug Port Specification Ver.1.0 —
it's used to read the D-port (POST) status codes over RS-232C (see
Section 6). A debug test cable + RS-232C crossover cable connect it to
a monitoring PC, and the D-port status is read with `GETDPORT.COM`
(MS-DOS, CPU REAL mode).

The rest of the connector list (for reference, with page numbers):
IS1420/IS1440 Memory1/2 (200-pin, C.1–C.2, p. C-1..C-8), PJ5206 LCD
(20-pin, C.3, p. C-9), PJ5620 CRT (15-pin, C.4), PJ4600/PJ4601 USB0/1
(4-pin, C.5–C.6), PJ1800 HDD (44-pin, C.7), PJ2110 PC Card (68-pin,
C.9), PJ3230 Keyboard (34-pin, C.10), PJ3240 PAD (8-pin, C.11),
PJ3280 Panel Sensor (2-pin, C.12), PJ4100 Network (14-pin, C.13),
PJ2200 Mini PCI (124-pin, C.14), PJ4410 Bluetooth (20-pin, C.15),
PJ8800 DC-IN (2-pin, C.16), PJ8810 1st Battery (10-pin, C.17), PJ8490
RTC battery (2-pin, C.18), PJ8400 FAN (3-pin, C.19), PJ6005 Speaker
(2-pin, C.20), PJ6003 Ext.Mic (6-pin, C.21), PJ6000 Int.Mic (6-pin,
C.22), PJ6004 Headphone (6-pin, C.23), PJ2130 SD (12-pin, C.24),
PJ3020 MDC (30-pin, C.25), PJ2300 Docking (240-pin, C.26), PJ3540
Digitizer SW (30-pin, C.27), PJ3261/PJ3291/PJ3290 board-to-board
(18/8/2-pin, C.28–C.30).

*(If you need the full pin table for any specific connector, I can
extract it separately — I didn't dump all of them here to keep the
file manageable.)*

---

## 6. BIOS Boot Process — D-Port (POST) Status Codes

This table comes from the system board fault-diagnosis section (Ch.2,
"System Board Troubleshooting") and shows **the real BIOS's IRT
(Initial ROM Test) flow step by step** — a direct reference for
mapping which D-port code corresponds to which stage while you boot
the BIOS in QEMU.

D-port read procedure: debug test cable → PJ2000 → RS-232C crossover
cable → monitoring PC. The computer is started in MS-DOS mode and
`GETDPORT.COM` is run in CPU REAL mode. **FFFFh = normal/complete.**

| D-port | Stage | Details | Note |
|---|---|---|---|
| **F000h** | Early init | Clear software reset bit, enable A20 line, init special registers and M1535, init PIT CH0 (HOLD_ON), init BIOS-rewrite flag, checksum check (FEFC0000h–FEFF7FFFh), switch to protected mode, Boot Block checksum, checksum of data outside Boot Block | HALT on checksum error |
| **F001h** | EC/KBC check | Check whether EC/KBC firmware needs rewriting, KBC init sequence, KBC enable command, check F12 key-in | Shifts to BIOS-rewrite flow if a rewrite request exists |
| **F002h** | Security | Security controller init | |
| **F003h–F005h** | BIOS rewrite check | Check whether a rewrite is requested | Checksum error outside Boot Block, or rewrite request via F12 key-in |
| **F006h** | | Check BIOS signature | |
| **F007h** | **BIOS Rewrite** | Shift to System BIOS IRT side, init model-specific HW, init GPIO I/O space, enable BIOS writing, serial interrupt control, cancel BIOS write protection, enable SMBus I/O/access, configure DRAM, enable L1 cache, clear memory | BIOS rewrite process |
| **F008h** | | Switch ROM BIOS to RAM BIOS | |
| **F009h** | Key/rewrite loop | Store key scan codes, set up TASK_1ms_TSC, display navipanel message (EC/KBC UPDATE, BIOS UPDATE/DAMAGED), load CHGBIOSA.EXE/CHGFIRMA.EXE (reset FDC, set parameters for 2HD/2DD, read root directory, read EXE header, execute) | Error → returns to key input |
| **F100h** | | Disable cache, init special registers, init PIT CH1 (refresh interval → 30µs) | |
| **F101h** | | Check DRAM size/type (on Cold Booting), test SMRAM stack area | DRAM=0 → HALT; HALT if stack area unusable |
| **F102h** | | Configure cache memory, enable L1, test CMOS access (Cold Booting), check CMOS backup battery, examine CMOS checksum, init CMOS data (1), set up IRT status, store DRAM size | |
| **F103h** | Resume branch | Check for resume (Cold Boot only), resume error checks: ICH4-M Power Failure (7Ah), SMRAM checksum (73h), memory configuration change (73h), system BIOS RAM checksum (79h), shift to RESUME_MAIN, resume error processing, disable all SMIs, copy system BIOS from ROM to RAM | HALT on copy error |
| **F104h** | | Init SMRAM, check WakeUp factor, rewrite SMRAM base + store CPU state map, enable SMI via ASMI | |
| **F106h** | PIT/CPU init | Init devices needed before PCI bus init, test/init PIT (Cold Boot), init and test channels 0/2/1, measure CPU clock, enable SMIs (except auto-off), control battery discharge current, **CPU init** (update P6 micro-code, processor serial number ID enable/disable, check Geyserville/SpeedStep support, switch CPU clock to high), set Graphics Aperture Size | PIT timeout → HALT |
| **F107h** | | Store ROM memory size in buffer, read EC version, update flash ROM type, determine destination from DMI data, check CMOS default (falls back to CMOS default on Bad Battery/Checksum), init ACPI table (for optional ROM), store VGA configuration results | |
| **F108h** | | Wait for PCI_CONFIGURATION completion, init HW needed after PCI config (enable/disable IEEE1394, generate output codes) | |
| **F10Ch** | | FIRST_64KB_CHECK | |
| **F10Dh** | | INIT_INT_VECTOR (init interrupt vectors) | |
| **F10Eh** | | INT_NDP (init NDP/FPU) | |
| **F10Fh** | | INIT_SYSTEM: store CMOS error info, init timer, init power-saving buffer, init EC + read battery information, update system BIOS (model name, LCD EDID) | |
| **F110h** | | INIT_DISPLAY: wait for VGA chip init to complete, pre-process VGA BIOS init | |
| **F111h** | | Init VGA BIOS | |
| **F112h** | | Post-processing after VGA BIOS init | |
| **F113h** | | DISP_LOGO (display logo) | |
| **F114h** | | SYS_MEM_CHECK (check conventional memory) | |
| **F115h** | | EXT_MEM_CHECK (check exception in protected mode) | |
| **F116h** | | CHK_DMA_PAGE | |
| **F117h** | | CHECK_DMA (check DMAC) | |
| **F11Ah** | | INT_DMAC (init DMAC) | |
| **F11Bh** | | BOOT_PASSWORD (check password), wait for end of divided FDD init (INT_FDD), wait for end of HDD init, check key input pressed during IRT (waits for KBC init to finish), init ATA prioritization | |
| **F11Eh** | | EX_IO_ROM_CHECK (check optional I/O ROM) | |
| **F11Fh** | **PRE_BOOT_SETUP** | Store value of 40:00 (for SIO save/restore), set up address of font data for resume password, set up parameters for USB keyboard character repeat, get keys pressed during IRT, store T_SHADOW_RAM_SIZE (shadow RAM size), update system resources info prior to boot-up, rewrite memory mapping data for INT15h E820h function, update DMI table, copy ACPI table to top of expansion memory, wait for completion of writing PSC version to BIOS, wait for completion of clock-generator setup (→ error: D-port = 1Eh), cancel NMI mask, calculate TIT checksum, clear IRT running flag for runtime, update runtime checksum, check whether CPU/HDD/other components have been upgraded, disable unused PC Card, init HW prior to BOOT (wait for completion), set up battery save mode, set up date, wait for completion of AC-Link init, update DMI Wakeup factor and SM-BIOS structure table, close PCI device configuration space, cache control, CPU-related processing, update parameter block A, set CPU clock speed to the value designated by Setup, wait for motor-off of disabled HDD, post-process PRE_BOOT_SETUP, clear power button status (PWRBTN), enable power button | On clock-generator error, process stops with D-port = 1Eh |
| **F121h** | | Check request for starting SETUP (via ESC key or CMOS error) | SETUP starts if a request exists |
| **FFFFh** | **Boot complete** | Normal status | |

### Additional D-port error status codes
| Code | Meaning |
|---|---|
| F160h | Timer CH2 error |
| F161h | PIT error |
| F162h | PIC #1 error |
| F163h | PIC #2 error |
| F120h | Clock generator setting error |

### CHGBIOSA.EXE / CHGFIRMA.EXE
If a BIOS-rewrite request is detected at D-port stage F009h, the BIOS
searches for `CHGBIOSA.EXE` (BIOS) and `CHGFIRMA.EXE` (EC/KBC
firmware) on the floppy (scanning the root directory, reading the EXE
header) and executes them. This shows exactly how the flash process is
triggered on real hardware — when replicating this flow in QEMU you'll
likely need FDD emulation plus these two files present on the disk
image.

---

## 7. BIOS/KBC/EC Update Procedure (Appendix F)

> Appendix F — "BIOS/KBC/EC Update" (F-1..F-2), short but clear:

1. Insert a floppy containing the update program into the FDD.
2. Power on the computer **while holding down the following key:**
   - **To update the BIOS:** `~` (Tilde key)
   - **To update the KBC/EC:** `Tab`
3. A beep sounds and a completion message is shown when the update
   finishes, and the system powers off automatically.
4. Verify the update succeeded using T&D (Test & Diagnostics).

> ⚠️ **CAUTION (from the original manual):** The power must **not** be
> turned off during the rewrite — if the write operation fails, the
> machine may not start up (brick risk).

This lines up exactly with the D-port flow: at boot, the BIOS checks
for F12 (or here, `~`/`Tab`), and if a rewrite is requested, copies the
BIOS from ROM to RAM and flashes it via CHGBIOSA.EXE/CHGFIRMA.EXE.

---

## 8. BIOS SETUP Program — Full Menu Structure (Chapter 3.26)

The SETUP program is entered by selecting "0-SETUP" from the
DIAGNOSTICS MENU. The screen is split across **2 pages** and covers
**16 functional groups**.

### Page 1/2 — Screen output (original, with ACPI BIOS version shown)

```
SYSTEM SETUP (1/2)                              ACPI BIOS version = X.XX

  MEMORY                                  BOOT PRIORITY
  Total = XXXXX KB                        Boot Priority = HDD→FDD→CD-ROM→LAN
                                           HDD Priority  = Built-in HDD→PC card
  SYSTEM DATE/TIME                        Network Boot Protocol = PXE
  System Date = XX-XX-XXXX
  System Time = XX:XX:XX                  DISPLAY
                                           Power On Display    = Auto-Selected
  BATTERY                                 LCD Display Stretch = Enabled
  Battery Save Mode = Full Power
                                           OTHERS
  PASSWORD                                CPU Cache = Enabled
  Not Registered                          Level 2 Cache = Enabled
                                           Dynamic CPU Frequency Mode
  HDD PASSWORD                                = Dynamically Switchable
  HDD               = Built-in HDD        Auto Power On = Disabled
  HDD Password Mode = User Only
  User Password     = Not Registered
  Master Password   = Not Registered

↑↓→←: Select items  Space/BkSp: Change values  PgDn/PgUp: Change pages
Esc: Exit without saving  Home: Set default values  End: Save changes and Exit
```

### Page 2/2

```
  CONFIGURATION                           PC CARD
  Device Config. = Setup by OS            Controller Mode = Auto-Selected

                                           PERIPHERAL
                                           Internal Pointing Device = Enabled
                                           Hard Disk Mode = Enhanced IDE(Normal)

  DRIVERS I/O                             LEGACY EMULATION
  Built-in HDD = Primary IDE(1F0H/IRQ14)  USB KB/Mouse Legacy Emulation = Enabled
  PC Card      = Others (190H/IRQ3)       USB-FDD Legacy Emulation = Enabled

  PCI BUS                                 PCI LAN
  PCI BUS = IRQ10, IRQ11                  Built-in LAN = Enabled

                                           SD
                                           SD Controller = Enabled
```

**Navigation:** `←/→` moves between columns, `↑/↓` moves within a
column, `Fn+↑(PgUp)/Fn+↓(PgDn)` moves between pages, `Space`/
`BackSpace` changes a value, `End` saves and exits, `Esc` exits without
saving, `Home` restores factory defaults (HDD mode, Password, and
Write Policy are not changed by Home).

### 16 groups — full option list

1. **Memory** — Total (read-only, calculated automatically)
2. **System Date/Time** — System Date (month-day-year), System Time (hh:mm)
3. **Battery** — Battery Save Mode: `Full Power` / `Low Power` / `User Setting`
   - Full Power defaults: Processing Speed=High, CPU Sleep=Enabled,
     Display Auto off=30Min, HDD Auto off=30Min, LCD Brightness=Bright/
     Super-Bright, Cooling Method=Maximum Performance
   - Low Power defaults: Processing Speed=Low, CPU Sleep=Enabled,
     Display Auto off=3Min, HDD Auto off=3Min, LCD Brightness=Semi-Bright/
     Bright, Cooling Method=Battery Optimized
   - **Processing Speed:** High=1.4/1.5/1.6/1.7GHz, Low=half speed
   - **CPU Sleep Mode:** Enabled(default)/Disabled
   - **Display Auto Off:** Disabled, or 1/3/5/10/15/20/30 min
   - **HDD Auto Off:** Disabled, or 1/3/5/10/15/20/30 min
   - **LCD Brightness:** Super-Bright/Bright/Semi-Bright
   - **Cooling Method:** Maximum Performance / Performance / Battery optimized
4. **Password** — Registered/Not registered (user password)
5. **HDD Password**
   - HDD: Built-in HDD (fixed)
   - HDD Password Mode: `User Only`(default) / `Master+User`
   - User Password / Master Password
6. **Boot Priority**
   - Boot Priority options: `FDD→HDD→CD-ROM→LAN`,
     `HDD→CD-ROM→LAN→FDD`, `FDD→CD-ROM→LAN→HDD`,
     `CD-ROM→LAN→HDD→FDD`, `CD-ROM→LAN→FDD→HDD`,
     **`HDD→FDD→CD-ROM→LAN` (default)**
   - HDD Priority: `Built-in HDD→PC Card`(default) / `PC Card→Built-in HDD`
   - Network Boot Protocol: `PXE`(default) / `RPL`
7. **Display**
   - Power On Display: `Auto-Selected`(default) / `LCD+Analog RGB` /
     `System LCD only`
   - LCD Display Stretch: `Enabled`(default) / `Disabled`
8. **Others**
   - CPU Cache: `Enabled`(default)/Disabled → sub-window: Write policy
     = `Write-back`(default) / `Write-through`
   - Level 2 Cache: `Enabled`(default)/Disabled (cannot be changed if
     CPU Cache is disabled)
   - Dynamic CPU Frequency Mode: `Dynamically Switchable`(default,
     SpeedStep active) / `Always High` / `Always Low`
   - Auto Power On: Disabled(default)/Enabled → sub-window: Alarm Time,
     Alarm Date Option, Wake-up on LAN (selectable if LAN is Enabled;
     requires AC adapter)
9. **Configuration** — Device Config.: `All Devices` / `Setup by OS`(default)
10. **Drives I/O** — Built-in HDD, PC Card (read-only info)
11. **PCI Bus** — read-only IRQ information
12. **PC Card** — Controller Mode: `Auto-Selected`(default) /
    `Card Bus/16-bit` / `PCIC Compatible`
13. **Peripheral**
    - Internal Pointing Device: `Enabled`(default)/Disabled
    - Hard Disk Mode: `Enhanced IDE (Normal)`(default, for Win XP
      Tablet PC Edition) / `Standard IDE` (528MB limit, different
      format)
14. **Legacy Emulation**
    - USB KB/Mouse Legacy Emulation: `Enabled`(default)/Disabled
    - USB-FDD Legacy Emulation: `Enabled`(default)/Disabled — **must be
      Enabled to boot from FD**
15. **PCI LAN** — Enabled(default)/Disabled
16. **SD** — SD Controller: Enabled(default)/Disabled

---

## 9. Power Supply and Voltage Rails

**Table 1-5 — Power supply output specifications** (26 different
voltage rails supplied to the system board):

| Name | Voltage [V] | Use |
|---|---|---|
| PPV | 1.000–0.748 | CPU |
| MCH1R2-P1V | 1.2 | MCH-M |
| PTV | 1.075 | CPU, MCH-M, ICH4-M |
| 2R5-P2V | 2.5 | GPU |
| 2R5-B2V | 2.5 | MCH-M, SDRAM |
| 1R25-P1V | 1.25 | MCH-M, SDRAM |
| PGV | 1.2–1.5 | GPU |
| LAN-E3V | 3.3 | ICH4-M, Kinnereth |
| S3V | 3.3 | ICH4-M, EC/KBC |
| B3V | 3.3 | YEBISU-SS, PC Card, MDC |
| P3V | 3.3 | Clock Gen, ADM1032, ICH4-M, FWH, Mini PCI, GPU, LCD, AD1981B, Super I/O |
| SD-B3V | 3.3 | SD |
| E5V | 5 | USB |
| B5V | 5 | PC card |
| P5V | 5 | ICH4-M, KB, LED, FL Inverter, HDD |
| IF-P5V | 5 | Dock |
| A4R7-P4V | 4.7 | AD1981B, Headphone, Int-Mic, Ext-Mic |
| 1R8-P1V | 1.8 | CPU, MCH-M, ICH4-M |
| 1R5-P1V | 1.5 | MCH-M, ICH4-M, GPU |
| 1R5-S1V | 1.5 | ICH4-M |
| LAN1R5-E1V | 1.5 | ICH4-M |
| M5V | 5 | LM26CIM5, LED |
| S5V | 5 | ICH4-M, Dock |
| MCV | 5 | PSC |
| R3V | 2.0–3.3 | ICH4-M |

The power microcontroller (PSC — TMP87PM48U) performs these functions:
detects AC adapter connection, detects DC output/short-circuit faults,
controls the battery/DC-IN LED, controls quick/trickle charging,
detects low battery, calculates remaining battery capacity,
transmits the battery status signal.

Battery/RTC battery, AC adapter specifications:

| | Value |
|---|---|
| Main battery | Li-Ion, 10.8V, 4400mAh (G71C0003D110/D210) |
| RTC battery | Li-Ion, 2.4V, 17mAh (GDM710000003) |
| RTC charging time | ~8 hours |
| RTC data preservation (fully charged) | 1 month |
| AC adapter | Input 100–240V AC (90-264V range), 50-60Hz, Output DC 15V, 0–4A |

Each voltage rail also has its own defined error-code range (e.g. S3V:
40h/45h, 1R5-C1V: 50h-55h, 1R8-C1V: 60h-65h, PPV: 70h-73h, PGV:
80h-84h, E5V: 90h-94h, E3V: A0h-A4h, 1R2-P1V: B0h-B3h, 2R5-P2V:
C0h-C4h, 1R25-P1V: D0h-D4h, 2R5-B2V: E0h-E4h) — the full list is in
the manual's 2.3 "Power Supply Troubleshooting" section (p. 2-9..2-14).

---

## 10. Diagnostics — Error Codes (Chapter 3)

### 3.19 Error Status Codes (for T&D tests)

| Device | Code | Meaning |
|---|---|---|
| Common | FF | Data Compare Error |
| System | 01/02/03 | ROM Checksum / Location ID / Serial ID Writer Error |
| Memory | 01/02/14/DD/DE/DF | Parity / Protected mode / R-W / Cache / 2nd Cache / TAG-RAM error |
| Keyboard | 01–06 | Mouse/IPS interface, retransmit, mouse handler errors |
| FDD | 01–EE | Bad command, address mark, write protect, CRC, seek, timeout, etc. |
| HDD | 01–DA | Bad command, ECC, seek, write fault, no HDD (DA), etc. |
| PCMCIA | C1–CD | Address/REG#/CE#/data/wait/BSY#/BVD1 line errors, no PCMCIA (CD) |
| NDP | 01–06 | No co-processor, control/status word, bus, addition, multiply error |

### DMI Read/Write (Subtest 07/08, System Test)
The DMI info stored in Flash ROM (Model Name, Version Number, Serial
Number, Model Number, PCN/BND Number, UUID Number) can be read/written.
The write procedure (Subtest 08) prompts in order for Model Name,
Version Number, Serial Number, Model Number, Bundle Number, and after
the "Write data OK (Y/N)?" confirmation, **writes to Flash ROM**.

---

## 11. Physical Disassembly/Reassembly (Chapter 4 — summary)

If you need physical access to the BIOS chip (IC3000, FWH) or the
EC/KBC (IC3200) on real hardware — to program them, for instance —
you'll need to remove the system board. Chapter 4 order (with page
references):

Battery pack (4-8) → PC card/SD card (4-10) → Keyboard (4-12) →
Touch pad (4-14) → Memory module (4-15) → HDD (4-17) → Base cover
assembly (4-19) → Mini PCI (4-21) → MDC modem (4-23) → FAN/CPU (4-25)
→ RTC battery (4-29) → Bluetooth module (4-30) → Hinge cables (4-32)
→ **System board (4-34)** → Speaker/LED board (4-36) → Second FAN
(4-39) → Pen holder/Battery lock/Base latch (4-40) → LCD unit/FL
inverter (4-43) → Application/Power switch boards/LCD latch (4-47) →
Digitizer (4-50) → LCD harness/Hinge assembly (4-56) → Hinge Switch
Board (4-59) → Fluorescent Lamp (4-60).

Screw tightening torque, screwdriver type (PH "0"/"1",
ISO/DIS 8764-1:1996), and safety notes before assembly are covered in
Section 4.1 (Safety Precautions, Tools and Equipment, Screw Tightening
Torque). Let me know if you'd like the full step-by-step text of this
section too (excluding the screenshots).

---

## 12. Notes for QEMU Emulation / Next Steps

- **BIOS chip map (Section 2)** will be your baseline reference for
  correctly placing region offsets (logo/setup/system BIOS/VGA-BIOS/
  ACPI/PnP/password/boot/LAN/reserved) when building a flash ROM image
  in QEMU.
- **The D-port table (Section 6)** lets you trace the real BIOS's IRT
  flow step by step inside QEMU — by simulating a POST card / D-port
  monitor (via a debug console or serial port), you can match exactly
  where you get stuck against this table.
- The chipset (855PM + ICH4-M) may not have an exact counterpart among
  QEMU's `-M` machine models; the closest starting point is generally
  one of the `pc-i440fx` variants, or a custom QEMU machine-type patch
  — this will be the biggest engineering piece of the project.
- Since the EC/KBC (Mitsubishi M306K5F8LRP, LPC) and the PSC (Toshiba
  TMP87PM48U) are separate microcontrollers, "BIOS" doesn't run alone
  on real hardware — booting only the System BIOS in QEMU will likely
  stall at steps that wait on EC/KBC init (F001h, F009h, F11Bh, and
  the KBC/PSC wait points inside F11Fh). You may need to stub/patch
  those points.
- The BIOS/EC update triggers (`~` = BIOS, `Tab` = KBC/EC) and the
  CHGBIOSA.EXE/CHGFIRMA.EXE flow are directly useful for understanding
  the original flash-write path for modding purposes.

---

*This document was compiled by scanning the entire 322-page original
PDF and filtering out every section directly relevant to BIOS/
firmware/hardware (Chapter 1 Hardware Overview, Chapter 2 System Board
& Power Supply Troubleshooting D-port table, Chapter 3.19 & 3.26
Diagnostics/SETUP, Chapter 4 Replacement summary, Appendix B Board
Layout, Appendix C Pin Assignment, Appendix F BIOS/KBC/EC Update).
Appendix A (LCD Module), D (Keyboard Scan Codes), E (Key Layout), G
(Reliability), and H (Key FD) are covered in Sections 13–16 below.*

---

## 13. Appendix A — LCD Module Handling Precautions (summary)

Not directly related to firmware, but relevant precautions for the
physical-modding stage:

1. When installing the LCD module in the cover, seat it so alignment
   and maximum display visibility are maintained.
2. Align the four corner holes without forcing the module into place —
   forcing it can affect panel performance; the polarized surface
   scratches easily.
3. If dirty, wipe with cotton or a soft cloth; if still dirty, breathe
   lightly on the surface to create condensate and wipe again; if very
   dirty, use a CRT cleaning agent (applied to the cloth, never
   directly to the panel).
4. If liquid sits on the panel for a long time it can discolor or
   stain it — wipe it off quickly.
5. The panel contains glass — handle carefully to avoid impacts/drops.
6. CMOS-LSI circuits are used inside — guard against ESD; wear a wrist
   or ankle ground strap.
7. Do not expose to direct sunlight or strong UV for extended periods.
8. Do not store below the specified temperature (cold can freeze the
   liquid crystals).
9. Do not disassemble the LCD module — it can cause malfunctions.
10. When transporting, do not use packing material containing epoxy
    resin (amine) or silicone glue (alcohol or oxime) — the gases
    released can damage the panel's polarization.

---

## 14. Appendix D — Keyboard Scan/Character Codes

Standard PS/2 **Scan Code Set 1** and **Set 2** (Make/Break codes), in
the standard IBM-compatible layout. QEMU keyboard emulation shouldn't
need anything special here (standard PS/2 controller behavior), but
it's a useful reference when debugging the physical keyboard matrix.
Sample rows (full table spans D-1..D-4, 110+ keys):

| Keytop | Set1 Make/Break | Set2 Make/Break | Note |
|---|---|---|---|
| ` ~ | 29 / A9 | 0E / F0 0E | |
| 1 ! | 02 / 82 | 16 / F0 16 | |
| Tab | 0F / 8F | 0D / F0 0D | |
| Q | 10 / 90 | 15 / F0 15 | |
| Caps Lock | 3A / BA | 58 / F0 58 | |
| Enter | 1C / 9C | 5A / F0 5A | *3 |
| Shift(L) | 2A / AA | 12 / F0 12 | |
| Shift(R) | 36 / B6 | 59 / F0 59 | |
| Ctrl | 1D / 9D | 14 / F0 14 | *3 |
| Alt(L) | 38 / B8 | 11 / F0 11 | *3 |
| Space | 39 / B9 | 29 / F0 29 | |
| Alt(R) | E0 38 / E0 B8 | E0 11 / E0 F0 11 | |
| ↑ / ↓ / ← / → | E0 48/50/4B/4D ... | E0 75/72/6B/74 ... | *1 (extended) |
| Ins / Del | E0 52/53 | E0 70/71 | *1 |
| Home / End | E0 47/4F | E0 6C/69 | *1 |
| PgUp / PgDn | E0 49/51 | E0 7D/7A | *1 |
| Esc | 01 / 81 | 76 / F0 76 | |
| F1–F10 | 3B..44 / BB..C4 | 05,06,04,0C,03,0B,83,0A,01,09 | F10: *3 |

*(Notes: \*1 = extended/E0-prefixed keys, \*2 = keys shared with the
numpad in some layouts, \*3 = special function/modifier keys. Happy to
extract the full 110+ row table if you need it — since this is a
standard PS/2 scan set, it generally shouldn't require extra work on
the QEMU side.)*

The keyboard has 84(US)/85(UK) keys, is JIS-compliant, connects to the
system board via a membrane connector (PJ3230, 34-pin), and is
controlled by the EC/KBC (M306K5F8LRP) — meaning keyboard matrix
scanning happens at the EC firmware level, and PS/2 scan-code
generation is also the KBC's job.

---

## 15. Appendix G — Reliability (MTBF)

| Component | MTBF (hours) |
|---|---|
| System (overall) | 5,401.5 or more |

---

## 16. Appendix H — Creating a Key FD (Password Service Disk)

Procedure for creating a "key FD" (password service disk) used to
recover from a forgotten password — from within the SETUP program:

1. Start the SETUP program.
2. With the cursor on "Not Registered" under PASSWORD, press
   `Space`/`BackSpace` to open the password entry screen. (If a
   password is already registered, cancel the existing one first, then
   set a new one.)
3. Enter a password (up to 10 characters; `*` is shown as you type).
4. Press `Enter` — the first password is accepted, and the second
   entry is requested.
5. Enter the same password again, then press `Enter`. (If it doesn't
   match, you'll be asked to re-enter it.)
6. Once the password is set, press `Fn + →`. The following message
   appears:
   `Are you sure? (Y/N)` — `The changes you made will cause the system
   to reboot. Insert password service disk if necessary.`
7. Insert a floppy into the USB FDD and press `Y`. The following
   message appears:
   `Password Service Disk Type? (1:2HD, 2:2DD)` — press `1` for a 2HD
   disk, `2` for 2DD. Data is written to the disk (if no disk is
   inserted, the procedure ends here).
8. Once writing is complete: `Remove the password service disk, then
   press any key.` — remove the disk, press any key, and the procedure
   is complete.

> This disk is used to reset the BIOS password in a lost-password
> scenario — it may be useful for your modding/recovery work as well.

---

*This document was compiled from the entire 322-page original PDF.
Sections covered: Chapter 1 (Hardware Overview — full), Chapter 2
(System Board & Power Supply Troubleshooting, full D-port POST table),
Chapter 3.19 & 3.26 (Error Status Codes, SETUP — full), Chapter 4
(Replacement Procedures — order summary), Appendix A (LCD Module —
summary), Appendix B (Board Layout — full), Appendix C (Pin Assignment
— full connector list, critical pinouts in full, bulky RAM pin maps
condensed), Appendix D (Keyboard Scan Codes — sample + explanation),
Appendix F (BIOS/KBC/EC Update — full), Appendix G (Reliability —
full), Appendix H (Key FD — full). Appendix E (Key Layout, a physical
key-layout diagram) is image-heavy and wasn't transcribed to text — I
can pull that page as an image too if you'd like.*
