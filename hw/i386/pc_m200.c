/*
 * QEMU Toshiba Portege M200 System Emulator
 *
 * Copyright (c) 2024
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include CONFIG_DEVICES

#include "qemu/units.h"
#include "hw/i386/x86.h"
#include "hw/i386/pc.h"
#include "hw/i386/apic.h"
#include "hw/pci-host/i855pm.h"
#include "hw/southbridge/ich4m.h"
#include "hw/rtc/mc146818rtc.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_bus.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "target/i386/cpu.h"
#include "system/kvm.h"
#include "system/tcg.h"
#include "hw/i386/kvm/clock.h"
#include "hw/ide/pci.h"
#include "hw/firmware/smbios.h"
#include "hw/acpi/acpi.h"
#include "hw/usb/usb.h"
#include "hw/i2c/smbus_eeprom.h"
#include "hw/southbridge/ich9.h"
#include "hw/display/nv34m.h"
#include "hw/char/serial-isa.h"
#include "chardev/char.h"
#include "system/hw_accel.h"
#include "qemu/datadir.h"
#include "hw/core/loader.h"

/* Toshiba Portege M200 Motherboard Straps & Hardware ID (Port 0x85) */
static uint8_t toshiba_m200_port85 = 0x00;

static uint64_t toshiba_m200_io_read(void *opaque, hwaddr addr, unsigned size)
{
    return toshiba_m200_port85;
}

static void toshiba_m200_io_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    toshiba_m200_port85 = val & 0xff;
}

static const MemoryRegionOps toshiba_m200_io_ops = {
    .read = toshiba_m200_io_read,
    .write = toshiba_m200_io_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Toshiba Embedded Controller (Ports 0xEE/0xEF) */
typedef struct ToshibaECState {
    uint8_t index;
    uint8_t regs[256];
} ToshibaECState;

static uint64_t toshiba_ec_read(void *opaque, hwaddr addr, unsigned size)
{
    ToshibaECState *ec = opaque;
    if (addr == 0) {
        return ec->index;
    } else {
        return ec->regs[ec->index];
    }
}

static void toshiba_ec_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    ToshibaECState *ec = opaque;
    if (addr == 0) {
        ec->index = val & 0xff;
    } else {
        ec->regs[ec->index] = val & 0xff;
    }
}

static const MemoryRegionOps toshiba_ec_ops = {
    .read = toshiba_ec_read,
    .write = toshiba_ec_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* ACPI Embedded Controller (Ports 0x62 Data / 0x66 Status & Command) */
typedef struct ToshibaAcpiECState {
    uint8_t status;
    uint8_t cmd;
    uint8_t data_index;
} ToshibaAcpiECState;

static uint64_t toshiba_acpi_ec_data_read(void *opaque, hwaddr addr, unsigned size)
{
    ToshibaAcpiECState *ec = opaque;
    ec->status &= ~0x01; /* Clear OBF */
    return 0x01; /* EC Status / Power / Security Signature Valid (0x01) */
}

static void toshiba_acpi_ec_data_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    ToshibaAcpiECState *ec = opaque;
    ec->status |= 0x01; /* Set OBF (Output Buffer Full) */
}

static uint64_t toshiba_acpi_ec_cmd_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0x71; /* 0x70 = System Power ON, AC connected, Lid Open | 0x01 = OBF data ready */
}

static void toshiba_acpi_ec_cmd_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    ToshibaAcpiECState *ec = opaque;
    ec->cmd = val & 0xff;
    ec->status |= 0x01; /* Set OBF (Output Buffer Full) */
}

/* Toshiba Portege M200 Hardware Debug Port (PJ2000 - MS Debug Port Spec v1.0 on 0x1E0..0x1E7) */
typedef struct ToshibaDPortState {
    uint8_t byte_count;
    uint16_t current_code;
} ToshibaDPortState;

static const char *dport_code_name(uint16_t code)
{
    switch (code) {
    case 0xF000: return "Early init (A20, PIT CH0, BootBlock Checksum)";
    case 0xF001: return "EC/KBC check";
    case 0xF002: return "Security controller init";
    case 0xF007: return "BIOS Rewrite / Shift to System BIOS IRT";
    case 0xF008: return "Switch ROM BIOS to RAM BIOS";
    case 0xF009: return "Key/rewrite loop";
    case 0xF100: return "Init special registers, PIT CH1 refresh";
    case 0xF101: return "Check DRAM size/type, test SMRAM";
    case 0xF102: return "Configure cache, enable L1, test CMOS";
    case 0xF103: return "Resume branch / Copy System BIOS from ROM to RAM";
    case 0xF104: return "Init SMRAM, SMI setup";
    case 0xF106: return "PIT/CPU init, microcode, SpeedStep";
    case 0xF107: return "Read EC version, flash ROM type, DMI";
    case 0xF108: return "PCI Configuration";
    case 0xF10C: return "FIRST_64KB_CHECK";
    case 0xF10D: return "INIT_INT_VECTOR";
    case 0xF10E: return "INT_NDP (FPU init)";
    case 0xF10F: return "INIT_SYSTEM (EC, battery, LCD EDID)";
    case 0xF110: return "INIT_DISPLAY (VGA chip pre-init)";
    case 0xF111: return "INIT_VGA_BIOS (Execute NVIDIA VBIOS at C0000h)";
    case 0xF112: return "Post-processing after VGA BIOS init";
    case 0xF113: return "DISP_LOGO (Display authentic Toshiba logo)";
    case 0xF114: return "SYS_MEM_CHECK";
    case 0xF115: return "EXT_MEM_CHECK";
    case 0xF116: return "CHK_DMA_PAGE";
    case 0xF117: return "CHECK_DMA";
    case 0xF11A: return "INT_DMAC";
    case 0xF11B: return "BOOT_PASSWORD / FDD & HDD init";
    case 0xF11E: return "EX_IO_ROM_CHECK";
    case 0xF11F: return "PRE_BOOT_SETUP";
    case 0xF120: return "ERROR: Clock generator setting error";
    case 0xF121: return "Check request for starting SETUP";
    case 0xF160: return "ERROR: Timer CH2 error";
    case 0xF161: return "ERROR: PIT error";
    case 0xF162: return "ERROR: PIC #1 error";
    case 0xF163: return "ERROR: PIC #2 error";
    case 0xFFFF: return "Boot complete (Normal status)";
    default: return "NoteBIOS POST step";
    }
}

static uint64_t toshiba_dport_read(void *opaque, hwaddr addr, unsigned size)
{
    switch (addr) {
    case 5: /* LSR - Line Status Register */
        return 0x60; /* THRE & TEMT = 1 (Transmitter empty, ready for next byte) */
    case 6: /* MSR - Modem Status Register */
        return 0xB0; /* DSR, CTS, DCD active */
    default:
        return 0x00;
    }
}

static void toshiba_dport_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    ToshibaDPortState *dp = opaque;
    if (addr == 0) { /* THR - Transmit Holding Register */
        if (dp->byte_count == 0) {
            dp->current_code = (val & 0xff);
            dp->byte_count = 1;
        } else {
            dp->current_code |= ((val & 0xff) << 8);
            dp->byte_count = 0;
            fprintf(stderr, "\n>>> [TOSHIBA D-PORT PJ2000] 0x%04X : %s <<<\n",
                    dp->current_code, dport_code_name(dp->current_code));
        }
    }
}

static const MemoryRegionOps toshiba_dport_ops = {
    .read = toshiba_dport_read,
    .write = toshiba_dport_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps toshiba_acpi_ec_data_ops = {
    .read = toshiba_acpi_ec_data_read,
    .write = toshiba_acpi_ec_data_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps toshiba_acpi_ec_cmd_ops = {
    .read = toshiba_acpi_ec_cmd_read,
    .write = toshiba_acpi_ec_cmd_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Intel ICH4-M ACPI Power Management Timer (3.579545 MHz counter on Port 0xD808 / ACPI Base 0xD800) */
static uint32_t toshiba_pm_timer_val = 0;

static uint64_t toshiba_acpi_pm_read(void *opaque, hwaddr addr, unsigned size)
{
    if (addr >= 0x08 && addr < 0x0c) {
        toshiba_pm_timer_val = (toshiba_pm_timer_val + 50000) & 0x00ffffff;
        return toshiba_pm_timer_val;
    }
    return 0;
}

static void toshiba_acpi_pm_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
}

static const MemoryRegionOps toshiba_acpi_pm_ops = {
    .read = toshiba_acpi_pm_read,
    .write = toshiba_acpi_pm_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

/* Toshiba Super I/O & Serial Status Port 0x1E0..0x1E7 */
static uint64_t toshiba_sio_1e0_read(void *opaque, hwaddr addr, unsigned size)
{
    switch (addr) {
    case 5: /* Line Status Register (LSR) */
        return 0x60; /* THRE | TEMT (Transmitter Empty & Ready) */
    case 6: /* Modem Status Register (MSR) */
        return 0xb0; /* DSR | CTS | CD */
    default:
        return 0x00;
    }
}

static void toshiba_sio_1e0_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
}

static const MemoryRegionOps toshiba_sio_1e0_ops = {
    .read = toshiba_sio_1e0_read,
    .write = toshiba_sio_1e0_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

/* Toshiba LCD Display Controller Port 0xD800..0xD81F */
static uint64_t toshiba_lcd_d800_read(void *opaque, hwaddr addr, unsigned size)
{
    switch (addr) {
    case 0x00: return 0x0001; /* LCD Enabled */
    case 0x02: return 0x0000; /* Status Ready */
    case 0x08: return 0x0000; /* Transfer Ready */
    default: return 0x0000;
    }
}

static void toshiba_lcd_d800_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
}

static const MemoryRegionOps toshiba_lcd_d800_ops = {
    .read = toshiba_lcd_d800_read,
    .write = toshiba_lcd_d800_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

/* Intel ICH4-M APM / SMI Software Control (Ports 0xB2 / 0xB3) */
static uint64_t toshiba_apmc_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0x00;
}

static void toshiba_apmc_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    CPUState *cpu = first_cpu;
    if (!cpu) return;
    cpu_synchronize_state(cpu);
    X86CPU *x86_cpu = X86_CPU(cpu);
    CPUX86State *env = &x86_cpu->env;

    uint8_t cmd = val & 0xff;
    fprintf(stderr, ">>> [TOSHIBA SMM SMI TRIGGER PORT 0xB2] CMD=0x%02X <<<\n", cmd);

    if (cmd == 0xc1) {
        /* Toshiba SMM Signature 'TX\x00\x01' in EAX */
        env->regs[R_EAX] = 0x74580001;
        env->regs[R_EBX] = 0x00000000;
    }
}

static const MemoryRegionOps toshiba_apmc_ops = {
    .read = toshiba_apmc_read,
    .write = toshiba_apmc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 2,
    },
};

/* Port 0x64 KBC Status (Bit 2 = SYSF: System POST Self-Test Passed, Normal Boot) */
static uint64_t toshiba_kbc_status_read(void *opaque, hwaddr addr, unsigned size)
{
    fprintf(stderr, ">>> [TOSHIBA KBC READ PORT 0x64] -> 0x1C\n");
    return 0x1c;
}

static void toshiba_kbc_status_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
}

static const MemoryRegionOps toshiba_kbc_status_ops = {
    .read = toshiba_kbc_status_read,
    .write = toshiba_kbc_status_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

/* IVT Memory Intercept (0x00000000..0x000003FF) */
static uint8_t *toshiba_system_ram = NULL;

static uint64_t toshiba_ivt_read(void *opaque, hwaddr addr, unsigned size)
{
    if (size == 4) {
        switch (addr) {
        case 0x20: return 0xf000feb0; /* INT 08h -> System BIOS Timer Tick */
        case 0x24: return 0xf000d16d; /* INT 09h -> System BIOS Keyboard ISR */
        case 0x40: return 0xc000006f; /* INT 10h -> NVIDIA VBIOS Video Services */
        case 0x44: return 0xf000f84d; /* INT 11h -> Equipment List Services */
        case 0x48: return 0xf000f841; /* INT 12h -> Memory Size Services */
        case 0x4c: return 0xf000ec59; /* INT 13h -> Fixed Disk & Floppy Services */
        case 0x50: return 0xf000e739; /* INT 14h -> Serial Communications */
        case 0x54: return 0xf000f859; /* INT 15h -> System & ACPI E820 Services */
        case 0x58: return 0xf000e82e; /* INT 16h -> Keyboard Input Services */
        case 0x5c: return 0xf000efd2; /* INT 17h -> Parallel Printer Services */
        case 0x60: return 0xf000e2c3; /* INT 18h -> ROM BASIC / Boot Failure */
        case 0x64: return 0xf000e6f2; /* INT 19h -> Bootstrap OS Loader */
        case 0x68: return 0xf000fe6e; /* INT 1Ah -> Real-Time Clock Services */
        case 0x70: return 0xf000ff53; /* INT 1Ch -> User Timer Tick Hook */
        case 0x78: return 0xf000efc7; /* INT 1Eh -> Floppy Parameter Table */
        default: break;
        }
    }
    if (toshiba_system_ram && (addr + size) <= 1024) {
        uint64_t val = 0;
        memcpy(&val, toshiba_system_ram + addr, size);
        return val;
    }
    return 0;
}

static void toshiba_ivt_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    if (toshiba_system_ram && (addr + size) <= 1024) {
        memcpy(toshiba_system_ram + addr, &val, size);
    }
}

static const MemoryRegionOps toshiba_ivt_ops = {
    .read = toshiba_ivt_read,
    .write = toshiba_ivt_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static void toshiba_vga_set_char(int row, int col, uint8_t ch, uint8_t attr)
{
    if (row < 0 || row >= 25 || col < 0 || col >= 80) return;
    if (toshiba_system_ram) {
        toshiba_system_ram[0xb8000 + (row * 80 + col) * 2] = ch;
        toshiba_system_ram[0xb8000 + (row * 80 + col) * 2 + 1] = attr;
    }
    nv34m_vga_write_text(row, col, ch, attr);
}

static void toshiba_vga_int10_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    CPUState *cpu = first_cpu;
    if (!cpu || !toshiba_system_ram) return;
    cpu_synchronize_state(cpu);
    X86CPU *x86_cpu = X86_CPU(cpu);
    CPUX86State *env = &x86_cpu->env;

    uint8_t ah = (env->regs[R_EAX] >> 8) & 0xff;
    uint8_t al = env->regs[R_EAX] & 0xff;
    uint8_t bh = (env->regs[R_EBX] >> 8) & 0xff;
    uint8_t bl = env->regs[R_EBX] & 0xff;
    uint16_t cx = env->regs[R_ECX] & 0xffff;
    uint8_t dh = (env->regs[R_EDX] >> 8) & 0xff;
    uint8_t dl = env->regs[R_EDX] & 0xff;

    printf(">>> [INT 10h CALLED] AH=0x%02X AL=0x%02X BX=0x%04X CX=0x%04X DX=0x%04X\n",
           ah, al, (uint16_t)env->regs[R_EBX], cx, (uint16_t)env->regs[R_EDX]);

    fprintf(stderr, ">>> [INT 10h] AH=0x%02X AL=0x%02X BH=0x%02X BL=0x%02X CX=0x%04X DX=0x%04X\n",
            ah, al, bh, bl, cx, (dh << 8) | dl);

    switch (ah) {
    case 0x00: /* Set Video Mode */
        if (al == 0x12) {
            /* Mode 12h: 640x480 16-color graphics */
            toshiba_system_ram[0x449] = 0x12;
            *(uint16_t *)(toshiba_system_ram + 0x44a) = 80;
            toshiba_system_ram[0x484] = 29;
            nv34m_vga_set_mode(640, 480, 4);
        } else {
            /* Mode 03h: 80x25 text mode */
            toshiba_system_ram[0x449] = 0x03;
            *(uint16_t *)(toshiba_system_ram + 0x44a) = 80;
            toshiba_system_ram[0x484] = 24;
            *(uint16_t *)(toshiba_system_ram + 0x450) = 0x0000;
            nv34m_vga_set_mode(640, 400, 0);
            nv34m_vga_clear_screen(0x07);
        }
        env->regs[R_EAX] = (env->regs[R_EAX] & ~0xff) | 0x30;
        break;

    case 0x01: /* Set Cursor Type */
        *(uint16_t *)(toshiba_system_ram + 0x460) = cx;
        break;

    case 0x02: /* Set Cursor Position */
        toshiba_system_ram[0x450 + ((bh & 7) * 2)] = dl;
        toshiba_system_ram[0x451 + ((bh & 7) * 2)] = dh;
        break;

    case 0x03: /* Get Cursor Position */
        dl = toshiba_system_ram[0x450 + ((bh & 7) * 2)];
        dh = toshiba_system_ram[0x451 + ((bh & 7) * 2)];
        env->regs[R_EDX] = (env->regs[R_EDX] & ~0xffff) | (dh << 8) | dl;
        env->regs[R_ECX] = (env->regs[R_ECX] & ~0xffff) | 0x0607;
        env->regs[R_EAX] = (env->regs[R_EAX] & ~0xffff);
        break;

    case 0x06: /* Scroll Window Up */
    case 0x07: { /* Scroll Window Down */
        uint8_t lines = al;
        uint8_t attr = bh;
        uint8_t top = (env->regs[R_ECX] >> 8) & 0xff;
        uint8_t left = env->regs[R_ECX] & 0xff;
        uint8_t bottom = dh;
        uint8_t right = dl;

        if (lines == 0 || lines >= (bottom - top + 1)) {
            for (int r = top; r <= bottom && r < 25; r++) {
                for (int c = left; c <= right && c < 80; c++) {
                    toshiba_vga_set_char(r, c, ' ', attr);
                }
            }
        } else if (ah == 0x06) {
            for (int r = top; r <= bottom - lines && r < 25; r++) {
                for (int c = left; c <= right && c < 80; c++) {
                    uint8_t ch = toshiba_system_ram[0xb8000 + ((r + lines) * 80 + c) * 2];
                    uint8_t at = toshiba_system_ram[0xb8000 + ((r + lines) * 80 + c) * 2 + 1];
                    toshiba_vga_set_char(r, c, ch, at);
                }
            }
            for (int r = bottom - lines + 1; r <= bottom && r < 25; r++) {
                for (int c = left; c <= right && c < 80; c++) {
                    toshiba_vga_set_char(r, c, ' ', attr);
                }
            }
        }
        break;
    }

    case 0x08: { /* Read Character and Attribute at Cursor */
        dl = toshiba_system_ram[0x450 + ((bh & 7) * 2)];
        dh = toshiba_system_ram[0x451 + ((bh & 7) * 2)];
        if (dh < 25 && dl < 80) {
            uint8_t ch = toshiba_system_ram[0xb8000 + (dh * 80 + dl) * 2];
            uint8_t at = toshiba_system_ram[0xb8000 + (dh * 80 + dl) * 2 + 1];
            env->regs[R_EAX] = (env->regs[R_EAX] & ~0xffff) | (at << 8) | ch;
        }
        break;
    }

    case 0x09: /* Write Character and Attribute */
    case 0x0a: { /* Write Character Only */
        dl = toshiba_system_ram[0x450 + ((bh & 7) * 2)];
        dh = toshiba_system_ram[0x451 + ((bh & 7) * 2)];
        for (int k = 0; k < cx && (dl + k) < 80 && dh < 25; k++) {
            uint8_t attr = (ah == 0x09) ? bl : toshiba_system_ram[0xb8000 + (dh * 80 + dl + k) * 2 + 1];
            toshiba_vga_set_char(dh, dl + k, al, attr);
        }
        break;
    }

    case 0x0e: { /* Teletype (TTY) Character Output */
        dl = toshiba_system_ram[0x450];
        dh = toshiba_system_ram[0x451];
        if (al == '\r') {
            dl = 0;
        } else if (al == '\n') {
            dh++;
            if (dh >= 25) {
                for (int r = 0; r < 24; r++) {
                    for (int c = 0; c < 80; c++) {
                        uint8_t ch = toshiba_system_ram[0xb8000 + ((r + 1) * 80 + c) * 2];
                        uint8_t at = toshiba_system_ram[0xb8000 + ((r + 1) * 80 + c) * 2 + 1];
                        toshiba_vga_set_char(r, c, ch, at);
                    }
                }
                for (int c = 0; c < 80; c++) {
                    toshiba_vga_set_char(24, c, ' ', 0x07);
                }
                dh = 24;
            }
        } else if (al == '\b') {
            if (dl > 0) dl--;
        } else if (al == '\a') {
            /* Beep */
        } else {
            if (dh < 25 && dl < 80) {
                toshiba_vga_set_char(dh, dl, al, bl ? bl : 0x07);
            }
            dl++;
            if (dl >= 80) {
                dl = 0;
                dh++;
                if (dh >= 25) {
                    for (int r = 0; r < 24; r++) {
                        for (int c = 0; c < 80; c++) {
                            uint8_t ch = toshiba_system_ram[0xb8000 + ((r + 1) * 80 + c) * 2];
                            uint8_t at = toshiba_system_ram[0xb8000 + ((r + 1) * 80 + c) * 2 + 1];
                            toshiba_vga_set_char(r, c, ch, at);
                        }
                    }
                    for (int c = 0; c < 80; c++) {
                        toshiba_vga_set_char(24, c, ' ', 0x07);
                    }
                    dh = 24;
                }
            }
        }
        toshiba_system_ram[0x450] = dl;
        toshiba_system_ram[0x451] = dh;
        break;
    }

    case 0x0f: /* Get Video Mode */
        env->regs[R_EAX] = (env->regs[R_EAX] & ~0xffff) | (80 << 8) | (toshiba_system_ram[0x449] & 0x7f);
        env->regs[R_EBX] = (env->regs[R_EBX] & ~0xff00);
        break;

    case 0x11: /* Font Services */
        if (al == 0x30) {
            /* Return 8x16 font pointer at F000:FA6E */
            env->regs[R_ECX] = (env->regs[R_ECX] & ~0xffff) | 16;
            env->regs[R_EDX] = (env->regs[R_EDX] & ~0xffff) | 24;
            env->regs[R_EBP] = (env->regs[R_EBP] & ~0xffff) | 0xfa6e;
            env->segs[R_ES].base = 0x000f0000;
            env->segs[R_ES].selector = 0xf000;
        }
        break;

    case 0x12: /* Alternate Select */
        if (bl == 0x10) {
            env->regs[R_EBX] = (env->regs[R_EBX] & ~0xffff) | 0x0003;
            env->regs[R_ECX] = (env->regs[R_ECX] & ~0xffff) | 0x0009;
        }
        break;

    case 0x13: { /* Write String */
        hwaddr str_phys = env->segs[R_ES].base + (env->regs[R_EBP] & 0xffff);
        for (int k = 0; k < cx && (dl + k) < 80 && dh < 25; k++) {
            uint8_t c = toshiba_system_ram[str_phys + k];
            toshiba_vga_set_char(dh, dl + k, c, bl ? bl : 0x07);
        }
        if (al & 1) {
            toshiba_system_ram[0x450] = (dl + cx) % 80;
            toshiba_system_ram[0x451] = dh + (dl + cx) / 80;
        }
        break;
    }

    case 0x4f: { /* VESA VBE Services */
        switch (al) {
        case 0x00: { /* Return VBE Controller Information */
            hwaddr vbe_info = env->segs[R_ES].base + (env->regs[R_EDI] & 0xffff);
            if (toshiba_system_ram && (vbe_info + 512) <= (16 * MiB)) {
                memset(toshiba_system_ram + vbe_info, 0, 512);
                memcpy(toshiba_system_ram + vbe_info, "VESA", 4);
                *(uint16_t *)(toshiba_system_ram + vbe_info + 4) = 0x0300; /* VBE 3.0 */
                *(uint32_t *)(toshiba_system_ram + vbe_info + 6) = 0xc000001e; /* OEM String: 'IBM VGA Compatible' */
                *(uint32_t *)(toshiba_system_ram + vbe_info + 10) = 0x00000001; /* Capabilities: 8-bit DAC */
                /* Video mode pointer at 0xC000:0x0080 */
                static const uint16_t modes[] = {0x0101, 0x0103, 0x0105, 0x0112, 0x0115, 0x0118, 0xffff};
                memcpy(toshiba_system_ram + vbe_info + 14, modes, sizeof(modes));
                *(uint16_t *)(toshiba_system_ram + vbe_info + 18) = 512; /* 32MB VRAM (512 * 64KB blocks) */
            }
            env->regs[R_EAX] = (env->regs[R_EAX] & ~0xffff) | 0x004f;
            break;
        }
        case 0x01: { /* Return VBE Mode Information */
            hwaddr mode_info = env->segs[R_ES].base + (env->regs[R_EDI] & 0xffff);
            if (toshiba_system_ram && (mode_info + 256) <= (16 * MiB)) {
                memset(toshiba_system_ram + mode_info, 0, 256);
                *(uint16_t *)(toshiba_system_ram + mode_info + 0) = 0x00bb; /* Mode attributes */
                toshiba_system_ram[mode_info + 2] = 0x07;                   /* Window A attributes (Read/Write) */
                *(uint16_t *)(toshiba_system_ram + mode_info + 4) = 64;     /* 64K window granularity */
                *(uint16_t *)(toshiba_system_ram + mode_info + 6) = 64;     /* 64K window size */
                *(uint16_t *)(toshiba_system_ram + mode_info + 8) = 0xa000; /* Window A start segment */
                *(uint16_t *)(toshiba_system_ram + mode_info + 16) = 640;   /* Bytes per scanline */
                *(uint16_t *)(toshiba_system_ram + mode_info + 18) = 640;   /* X Resolution */
                *(uint16_t *)(toshiba_system_ram + mode_info + 20) = 480;   /* Y Resolution */
                toshiba_system_ram[mode_info + 24] = 1;                     /* Number of planes */
                toshiba_system_ram[mode_info + 25] = 8;                     /* 8 bits per pixel */
                toshiba_system_ram[mode_info + 27] = 4;                     /* Packed pixel memory model */
                *(uint32_t *)(toshiba_system_ram + mode_info + 40) = 0xfd000000; /* Linear Frame Buffer Base */
            }
            env->regs[R_EAX] = (env->regs[R_EAX] & ~0xffff) | 0x004f;
            break;
        }
        case 0x02: { /* Set VBE Mode */
            uint16_t mode = env->regs[R_EBX] & 0x01ff;
            if (mode == 0x0105) {
                nv34m_vga_set_mode(1024, 768, 8);
            } else if (mode == 0x0103) {
                nv34m_vga_set_mode(800, 600, 8);
            } else {
                nv34m_vga_set_mode(640, 480, 8);
            }
            toshiba_system_ram[0x449] = 0x5f; /* VBE Mode */
            env->regs[R_EAX] = (env->regs[R_EAX] & ~0xffff) | 0x004f;
            break;
        }
        default:
            env->regs[R_EAX] = (env->regs[R_EAX] & ~0xffff) | 0x004f;
            break;
        }
        break;
    }

    default:
        break;
    }
}

static uint64_t toshiba_vga_int10_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}

static const MemoryRegionOps toshiba_vga_int10_ops = {
    .read = toshiba_vga_int10_read,
    .write = toshiba_vga_int10_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 2,
    },
};

/* Intel ICH4-M SMBus Controller (Base 0xD880) & DDR SDRAM SPD EEPROM */
typedef struct ToshibaSMBusState {
    uint8_t hst_sts;
    uint8_t hst_cnt;
    uint8_t hst_cmd;
    uint8_t xmit_slva;
    uint8_t hst_d0;
} ToshibaSMBusState;

static const uint8_t ddr_spd_dimm0[128] = {
    [0] = 0x80, /* 128 bytes */
    [1] = 0x08, /* Rev 1.0 */
    [2] = 0x07, /* DDR SDRAM */
    [3] = 0x0d, /* 13 rows */
    [4] = 0x0a, /* 10 cols */
    [5] = 0x01, /* 1 bank/rank */
    [6] = 0x40, /* 64-bit data width */
    [7] = 0x00,
    [8] = 0x04, /* 2.5V */
    [9] = 0x60, /* 6.0 ns (DDR-333) */
    [10] = 0x70, /* 0.70 ns */
    [11] = 0x00, /* Non-ECC */
    [12] = 0x82,
    [13] = 0x08,
    [14] = 0x00,
    [15] = 0x01,
    [16] = 0x0e,
    [17] = 0x04,
    [18] = 0x1c, /* CAS 2.0, 2.5, 3.0 */
    [19] = 0x01,
    [20] = 0x02,
    [21] = 0x20,
    [22] = 0x00,
    [23] = 0x60, /* tCK at CAS 2.0 = 6.0 ns (DDR-333) */
    [24] = 0x70, /* tAC at CAS 2.0 = 0.70 ns */
    [25] = 0x60, /* tCK at CAS 1.5 = 6.0 ns (DDR-333) */
    [26] = 0x70, /* tAC at CAS 1.5 = 0.70 ns */
    [27] = 0x30, /* tRP 12ns */
    [28] = 0x30, /* tRCD 12ns */
    [29] = 0x28, /* tRAS 40ns */
    [30] = 0x1e, /* tRAS min 30ns */
    [31] = 0x40, /* 512MB */
};

static uint64_t toshiba_smbus_read(void *opaque, hwaddr addr, unsigned size)
{
    ToshibaSMBusState *s = opaque;
    uint8_t val = 0;
    switch (addr) {
    case 0x00: /* HST_STS */
        val = s->hst_sts;
        break;
    case 0x02: /* HST_CNT */
        val = s->hst_cnt;
        break;
    case 0x03: /* HST_CMD */
        val = s->hst_cmd;
        break;
    case 0x04: /* XMIT_SLVA */
        val = s->xmit_slva;
        break;
    case 0x05: /* HST_D0 */
        val = s->hst_d0;
        break;
    default:
        val = 0;
        break;
    }
    fprintf(stderr, ">>> [TOSHIBA SMBUS READ] Port 0x%04X -> 0x%02X\n", (unsigned)(0xD880 + addr), val);
    return val;
}

static void toshiba_smbus_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    ToshibaSMBusState *s = opaque;
    fprintf(stderr, ">>> [TOSHIBA SMBUS WRITE] Port 0x%04X <- 0x%02X\n", (unsigned)(0xD880 + addr), (uint8_t)val);
    switch (addr) {
    case 0x00: /* HST_STS (W1C) */
        s->hst_sts &= ~(val & 0xff);
        break;
    case 0x02: /* HST_CNT */
        s->hst_cnt = val & 0xff;
        if (val & 0x40) { /* START */
            uint8_t dev_addr = s->xmit_slva >> 1;
            if (dev_addr == 0x50) { /* DIMM 0 (Slave 0xA0/0xA1) */
                uint8_t offset = s->hst_cmd & 0x7f;
                s->hst_d0 = ddr_spd_dimm0[offset];
                s->hst_sts = 0x02; /* INTR (Transaction Complete) */
                fprintf(stderr, ">>> [TOSHIBA SMBUS] DIMM 0 Read SPD[0x%02X] = 0x%02X\n", offset, s->hst_d0);
            } else {
                s->hst_sts = 0x04; /* DEV_ERR (No device at address) */
                fprintf(stderr, ">>> [TOSHIBA SMBUS] Slave 0x%02X (dev 0x%02X) NOT PRESENT\n", s->xmit_slva, dev_addr);
            }
        }
        break;
    case 0x03: /* HST_CMD */
        s->hst_cmd = val & 0xff;
        break;
    case 0x04: /* XMIT_SLVA */
        s->xmit_slva = val & 0xff;
        break;
    case 0x05: /* HST_D0 */
        s->hst_d0 = val & 0xff;
        break;
    }
}

static const MemoryRegionOps toshiba_smbus_ops = {
    .read = toshiba_smbus_read,
    .write = toshiba_smbus_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

/* Portege M200 hardware initialization */
static void m200_init(MachineState *machine)
{
    PCMachineState *pcms = PC_MACHINE(machine);
    PCMachineClass *pcmc = PC_MACHINE_GET_CLASS(pcms);
    X86MachineState *x86ms = X86_MACHINE(machine);
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *system_io = get_system_io();
    Object *phb = NULL;
    PCIDevice *lpc;
    MemoryRegion *ram_memory;
    MemoryRegion *pci_memory = NULL;
    GSIState *gsi_state;
    ISABus *isa_bus;
    PCIDevice *pci_ide;
    DriveInfo *hd[MAX_IDE_BUS * MAX_IDE_DEVS];
    ram_addr_t lowmem;
    int i;

    /* Memory setup - M200 supports up to 2GB DDR-333 */
    if (!pcms->max_ram_below_4g) {
        pcms->max_ram_below_4g = 0x80000000; /* 2GB default */
    }
    lowmem = pcms->max_ram_below_4g;

    if (machine->ram_size >= lowmem) {
        x86ms->above_4g_mem_size = machine->ram_size - lowmem;
        x86ms->below_4g_mem_size = lowmem;
    } else {
        x86ms->above_4g_mem_size = 0;
        x86ms->below_4g_mem_size = machine->ram_size;
    }

    ram_memory = machine->ram;

    /* Initialize CPUs */
    x86_cpus_init(x86ms, pcmc->default_cpu_version);

    if (kvm_enabled()) {
        kvmclock_create(pcmc->kvmclock_create_always);
    }

    /* PCI Host Bridge (i855PM MCH + AGP Bridge) */
    pci_memory = g_new(MemoryRegion, 1);
    memory_region_init(pci_memory, NULL, "pci", UINT64_MAX);

    phb = OBJECT(qdev_new(TYPE_I855PM_HOST_DEVICE));
    object_property_add_child(OBJECT(machine), "i855pm", phb);
    object_property_set_link(phb, PCI_HOST_PROP_RAM_MEM,
                             OBJECT(ram_memory), &error_fatal);
    object_property_set_link(phb, PCI_HOST_PROP_PCI_MEM,
                             OBJECT(pci_memory), &error_fatal);
    object_property_set_link(phb, PCI_HOST_PROP_SYSTEM_MEM,
                             OBJECT(system_memory), &error_fatal);
    object_property_set_link(phb, PCI_HOST_PROP_IO_MEM,
                             OBJECT(system_io), &error_fatal);
    object_property_set_uint(phb, PCI_HOST_BELOW_4G_MEM_SIZE,
                             x86ms->below_4g_mem_size, &error_fatal);
    object_property_set_uint(phb, PCI_HOST_ABOVE_4G_MEM_SIZE,
                             x86ms->above_4g_mem_size, &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(phb), &error_fatal);

    pcms->pcibus = PCI_BUS(qdev_get_child_bus(DEVICE(phb), "pci.0"));

    /* Memory initialization */
    pc_memory_init(pcms, system_memory, system_memory, 0);

    /* GSI (Global System Interrupts) */
    gsi_state = pc_gsi_create(&x86ms->gsi, true);

    /* ICH4M LPC Bridge */
    lpc = pci_new_multifunction(PCI_DEVFN(ICH4M_LPC_DEV, ICH4M_LPC_FUNC),
                                TYPE_ICH4M_LPC_DEVICE);
    pci_realize_and_unref(lpc, pcms->pcibus, &error_fatal);

    for (i = 0; i < IOAPIC_NUM_PINS; i++) {
        qdev_connect_gpio_out_named(DEVICE(lpc), ICH4M_GPIO_GSI, i, x86ms->gsi[i]);
    }

    x86ms->rtc = ISA_DEVICE(object_resolve_path_component(OBJECT(lpc), "rtc"));

    isa_bus = ISA_BUS(qdev_get_child_bus(DEVICE(lpc), "isa.0"));

    if (x86ms->pic == ON_OFF_AUTO_ON || x86ms->pic == ON_OFF_AUTO_AUTO) {
        pc_i8259_create(isa_bus, gsi_state->i8259_irq);
    }

    ioapic_init_gsi(gsi_state, OBJECT(phb));

    if (tcg_enabled()) {
        x86_register_ferr_irq(x86ms->gsi[13]);
    }

    /* Basic PC hardware (RTC, PIT, PC speaker, i8042 kbd/mouse, serial, parallel) */
    pc_basic_device_init(pcms, isa_bus, x86ms->gsi, x86ms->rtc, !mc->no_floppy,
                         0xff0104);

    /* Wacom ISDV4 Serial Tablet PC Digitizer on COM4 (I/O 0x0338, IRQ 4) */
    Chardev *wacom_chr = qemu_chardev_new(NULL, "chardev-wctablet", NULL, NULL, &error_fatal);
    ISADevice *wacom_serial = isa_new(TYPE_ISA_SERIAL);
    qdev_prop_set_uint32(DEVICE(wacom_serial), "iobase", 0x0338);
    qdev_prop_set_uint32(DEVICE(wacom_serial), "irq", 4);
    qdev_prop_set_chr(DEVICE(wacom_serial), "chardev", wacom_chr);
    isa_realize_and_unref(wacom_serial, isa_bus, &error_fatal);

    /* ICH4-M IDE Controller (D31:F1) */
    pci_ide = pci_create_simple_multifunction(pcms->pcibus,
                                             PCI_DEVFN(ICH4M_IDE_DEV, ICH4M_IDE_FUNC),
                                             "piix4-ide");
    pci_config_set_vendor_id(pci_ide->config, PCI_VENDOR_ID_INTEL);
    pci_config_set_device_id(pci_ide->config, PCI_DEVICE_ID_INTEL_82801DBM_IDE);
    pci_config_set_revision(pci_ide->config, 0x03);
    pci_set_word(pci_ide->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
    pci_set_word(pci_ide->config + PCI_SUBSYSTEM_ID, 0x0001);
    pcms->idebus[0] = qdev_get_child_bus(DEVICE(pci_ide), "ide.0");
    pcms->idebus[1] = qdev_get_child_bus(DEVICE(pci_ide), "ide.1");
    ide_drive_get(hd, ARRAY_SIZE(hd));
    pci_ide_create_devs(pci_ide);

    /* USB UHCI & EHCI (D29:F0, F1, F2, F7) */
    PCIDevice *u1 = pci_create_simple_multifunction(pcms->pcibus, PCI_DEVFN(29, 0), "piix4-usb-uhci");
    PCIDevice *u2 = pci_create_simple_multifunction(pcms->pcibus, PCI_DEVFN(29, 1), "piix4-usb-uhci");
    PCIDevice *u3 = pci_create_simple_multifunction(pcms->pcibus, PCI_DEVFN(29, 2), "piix4-usb-uhci");
    PCIDevice *ehci = pci_create_simple_multifunction(pcms->pcibus, PCI_DEVFN(29, 7), "usb-ehci");
    pci_config_set_device_id(u1->config, PCI_DEVICE_ID_INTEL_82801DBM_UHCI1);
    pci_config_set_device_id(u2->config, PCI_DEVICE_ID_INTEL_82801DBM_UHCI2);
    pci_config_set_device_id(u3->config, PCI_DEVICE_ID_INTEL_82801DBM_UHCI3);
    pci_config_set_device_id(ehci->config, PCI_DEVICE_ID_INTEL_82801DBM_EHCI);
    pci_set_word(u1->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
    pci_set_word(u1->config + PCI_SUBSYSTEM_ID, 0x0001);
    pci_set_word(u2->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
    pci_set_word(u2->config + PCI_SUBSYSTEM_ID, 0x0001);
    pci_set_word(u3->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
    pci_set_word(u3->config + PCI_SUBSYSTEM_ID, 0x0001);
    pci_set_word(ehci->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
    pci_set_word(ehci->config + PCI_SUBSYSTEM_ID, 0x0001);

    /* Intel 82801 Mobile PCI Bridge (D30:F0) - 8086:2448 rev 83 */
    PCIDevice *pci_br = pci_create_simple_multifunction(pcms->pcibus,
                                                       PCI_DEVFN(30, 0),
                                                       "i82801b11-bridge");
    pci_config_set_device_id(pci_br->config, PCI_DEVICE_ID_INTEL_82801_PCI_BR);
    pci_config_set_revision(pci_br->config, 0x83);

    /* ICH4-M SMBus Controller (D31:F3) - 8086:24c3 rev 03, Subsystem 1179:0001 */
    if (pcms->smbus_enabled) {
        PCIDevice *smb = pci_create_simple_multifunction(pcms->pcibus,
                                                        PCI_DEVFN(ICH4M_SMB_DEV, ICH4M_SMB_FUNC),
                                                        TYPE_ICH9_SMB_DEVICE);
        pci_config_set_vendor_id(smb->config, PCI_VENDOR_ID_INTEL);
        pci_config_set_device_id(smb->config, PCI_DEVICE_ID_INTEL_82801DBM_SMB);
        pci_config_set_revision(smb->config, 0x03);
        pci_set_word(smb->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
        pci_set_word(smb->config + PCI_SUBSYSTEM_ID, 0x0001);
        pcms->smbus = I2C_BUS(qdev_get_child_bus(DEVICE(smb), "i2c"));
        smbus_eeprom_init(pcms->smbus, 8, NULL, 0);
    }

    /* AC'97 Audio (D31:F5) */
    if (pcmc->pci_enabled) {
        PCIDevice *audio = pci_create_simple_multifunction(pcms->pcibus,
                                                          PCI_DEVFN(ICH4M_AC97_DEV, ICH4M_AC97_FUNC),
                                                          "AC97");
        pci_config_set_vendor_id(audio->config, PCI_VENDOR_ID_INTEL);
        pci_config_set_device_id(audio->config, PCI_DEVICE_ID_INTEL_82801DBM_AUDIO);
        pci_config_set_revision(audio->config, 0x03);
        pci_set_word(audio->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
        pci_set_word(audio->config + PCI_SUBSYSTEM_ID, 0x02c5);
    }

    /* Video (NVIDIA GeForce FX Go5200 / NV34M) & Network */
    pci_create_simple(pcms->pcibus, PCI_DEVFN(2, 0), TYPE_NV34M_VGA);
    pc_nic_init(pcmc, isa_bus, pcms->pcibus);

    /* Map NVIDIA GeForce FX Go5200 VBIOS at 0x000C0000 */
    char *vbios_file = qemu_find_file(QEMU_FILE_TYPE_BIOS, "nv34m_vbios.bin");
    if (vbios_file) {
        int vbios_size = get_image_size(vbios_file, NULL);
        if (vbios_size > 0 && vbios_size <= 0x10000) {
            MemoryRegion *vbios_rom = g_new(MemoryRegion, 1);
            memory_region_init_ram(vbios_rom, NULL, "m200.vbios", 0x10000, &error_fatal);
            load_image_size(vbios_file, memory_region_get_ram_ptr(vbios_rom), vbios_size);
            memory_region_add_subregion_overlap(system_memory, 0xc0000, vbios_rom, 1000);
        }
        g_free(vbios_file);
    }

    /* Map 64KB Shadow RAM at 0x000E0000 (preloaded with Module 5, writeable for Option ROM init) */
    MemoryRegion *shadow_ram_e0000 = g_new(MemoryRegion, 1);
    memory_region_init_ram(shadow_ram_e0000, NULL, "m200.shadow-e0000", 0x10000, &error_fatal);
    if (machine->firmware) {
        char *filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, machine->firmware);
        if (filename) {
            FILE *f = fopen(filename, "rb");
            if (f) {
                fseek(f, 0x60000, SEEK_SET); /* Module 5 offset in 512KB ROM */
                if (fread(memory_region_get_ram_ptr(shadow_ram_e0000), 1, 0x10000, f) == 0x10000) {
                    /* Successfully preloaded Module 5 */
                }
                fclose(f);
            }
            g_free(filename);
        }
    }
    memory_region_add_subregion_overlap(system_memory, 0xe0000, shadow_ram_e0000, 1000);

    /* Map 64KB Shadow RAM at 0x000F0000 (preloaded with Module 2 System BIOS, writeable for Stage 0x03) */
    MemoryRegion *shadow_ram_f0000 = g_new(MemoryRegion, 1);
    memory_region_init_ram(shadow_ram_f0000, NULL, "m200.shadow-f0000", 0x10000, &error_fatal);
    if (machine->firmware) {
        char *filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, machine->firmware);
        if (filename) {
            FILE *f = fopen(filename, "rb");
            if (f) {
                uint8_t *ram = (uint8_t *)memory_region_get_ram_ptr(machine->ram);
                /* Preload Module 3 (POST Kernel Screen Writer) at 0x30000 */
                fseek(f, 0x30000, SEEK_SET);
                if (fread(ram + 0x30000, 1, 0x10000, f) != 0x10000) {}
                /* Preload Module 4 (Setup Engine) at 0x40000 */
                fseek(f, 0x40000, SEEK_SET);
                if (fread(ram + 0x40000, 1, 0x10000, f) != 0x10000) {}
                /* Preload Module 5 (Configuration Tables) at 0x50000 */
                fseek(f, 0x50000, SEEK_SET);
                if (fread(ram + 0x50000, 1, 0x10000, f) != 0x10000) {}
                /* Preload Module 2 System BIOS into 0xF0000 */
                fseek(f, 0x20000, SEEK_SET);
                if (fread(memory_region_get_ram_ptr(shadow_ram_f0000), 1, 0x10000, f) == 0x10000) {
                    /* Successfully preloaded System BIOS */
                }
                /* Place Boot Block reset jump at 0x000FFFF0 so CPU starts in Boot Block */
                uint8_t *ptr = (uint8_t *)memory_region_get_ram_ptr(shadow_ram_f0000);
                ptr[0xfff0] = 0xea;
                ptr[0xfff1] = 0xa0;
                ptr[0xfff2] = 0x3f;
                ptr[0xfff3] = 0x00;
                ptr[0xfff4] = 0xfc;
                fclose(f);
            }
            g_free(filename);
        }
    }
    memory_region_add_subregion_overlap(system_memory, 0xf0000, shadow_ram_f0000, 1000);

    /* Map Video BIOS INT 10h Service Port (0x180) */
    MemoryRegion *vga_int10_io = g_new(MemoryRegion, 1);
    memory_region_init_io(vga_int10_io, OBJECT(machine), &toshiba_vga_int10_ops, NULL,
                          "m200-vga-int10", 2);
    memory_region_add_subregion_overlap(system_io, 0x180, vga_int10_io, 100);

    /* Map Toshiba Super I/O / Serial Status Port (0x1E0..0x1E7) */
    MemoryRegion *sio_1e0_mr = g_new(MemoryRegion, 1);
    memory_region_init_io(sio_1e0_mr, OBJECT(machine), &toshiba_sio_1e0_ops, NULL,
                          "toshiba-sio-1e0", 8);
    memory_region_add_subregion_overlap(system_io, 0x1e0, sio_1e0_mr, 100);

    /* Map Intel ICH4-M APM / Toshiba SMI Control Port (0xB2/0xB3) */
    MemoryRegion *apmc_io = g_new(MemoryRegion, 1);
    memory_region_init_io(apmc_io, OBJECT(machine), &toshiba_apmc_ops, NULL,
                          "toshiba-apmc-smi", 2);
    memory_region_add_subregion_overlap(system_io, 0xb2, apmc_io, 100);

    /* Map Toshiba LCD Display Controller Port (0xD800..0xD81F) */
    MemoryRegion *lcd_d800_mr = g_new(MemoryRegion, 1);
    memory_region_init_io(lcd_d800_mr, OBJECT(machine), &toshiba_lcd_d800_ops, NULL,
                          "toshiba-lcd-d800", 0x20);
    memory_region_add_subregion_overlap(system_io, 0xd800, lcd_d800_mr, 100);

    /* Initialize BDA (0x00000400..0x000004FF) and System Parameters */
    uint8_t *ram_base = (uint8_t *)memory_region_get_ram_ptr(machine->ram);
    *(uint16_t *)(ram_base + 0xa0) = 0x0000; /* Video Text Framebuffer Offset */
    *(uint16_t *)(ram_base + 0xa2) = 0xb800; /* Video Text Framebuffer Segment (0x000B8000) */
    *(uint16_t *)(ram_base + 0x400) = 0x03f8; /* COM1 */
    *(uint16_t *)(ram_base + 0x402) = 0x02f8; /* COM2 */
    *(uint16_t *)(ram_base + 0x404) = 0x03e8; /* COM3 */
    *(uint16_t *)(ram_base + 0x406) = 0x0338; /* COM4 (Wacom Tablet Digitizer) */
    *(uint16_t *)(ram_base + 0x408) = 0x0378; /* LPT1 */
    *(uint16_t *)(ram_base + 0x40e) = 0x00c0; /* EBDA Base Segment (0x00C0 -> 0x00000C00) */
    ram_base[0xc00] = 0x01;                   /* EBDA Size = 1 KB */
    *(uint16_t *)(ram_base + 0xe00) = 0xffff; /* Option ROM Table Terminator (Segment 0x00E0) */
    *(uint16_t *)(ram_base + 0x410) = 0x4423; /* Equipment word */
    *(uint16_t *)(ram_base + 0x413) = 640;    /* 640KB Base Memory */
    ram_base[0x449] = 0x03;                   /* Mode 03h (80x25 Color Text) */
    *(uint16_t *)(ram_base + 0x44a) = 80;     /* 80 Columns */
    *(uint16_t *)(ram_base + 0x44c) = 4096;   /* Page size */
    memset(ram_base + 0x450, 0, 16);          /* Cursor positions */
    *(uint32_t *)(ram_base + 0x46c) = 0;      /* Timer ticks */
    *(uint16_t *)(ram_base + 0x472) = 0x0000; /* Reset word (Normal Boot) */
    ram_base[0x484] = 24;                     /* Rows minus 1 */
    ram_base[0x485] = 16;                     /* Character height 16 */

    /* Initialize System Parameter Block at 0x00000800..0x000008FF */
    *(uint16_t *)(ram_base + 0x818) = 0xe05b;
    *(uint16_t *)(ram_base + 0x81a) = 0xf000;
    *(uint16_t *)(ram_base + 0x82c) = 0x1000;
    *(uint32_t *)(ram_base + 0x83c) = 0x000f0000;
    *(uint32_t *)(ram_base + 0x842) = 0x00007000;

    /* Initialize Interrupt Vector Table (IVT) Overlay (0x00000000..0x000003FF) */
    MemoryRegion *ivt_mr = g_new(MemoryRegion, 1);
    memory_region_init_io(ivt_mr, OBJECT(machine), &toshiba_ivt_ops, NULL, "m200-ivt", 1024);
    memory_region_add_subregion_overlap(system_memory, 0x00000000, ivt_mr, 1000);

    uint32_t *ivt = (uint32_t *)memory_region_get_ram_ptr(machine->ram);
    for (i = 0; i < 256; i++) {
        ivt[i] = 0xf000ff53; /* F000:FF53 (Default IRET) */
    }
    /* Point Master PIC hardware IRQ 0..7 (INT 08h..0Fh) to EOI handler at F000:FF40 */
    for (i = 0x08; i <= 0x0f; i++) {
        ivt[i] = 0xf000ff40;
    }
    /* Point Slave PIC hardware IRQ 8..15 (INT 70h..77h) to EOI handler at F000:FF30 */
    for (i = 0x70; i <= 0x77; i++) {
        ivt[i] = 0xf000ff30;
    }
    ivt[0x08] = 0xf000feb0; /* F000:FEB0 (Authentic Toshiba System BIOS Timer Tick) */
    ivt[0x09] = 0xf000d16d; /* F000:D16D (Authentic Toshiba System BIOS Keyboard Handler) */
    ivt[0x10] = 0xc000006f; /* C000:006F (NVIDIA VBIOS INT 10h Video Service) */
    ivt[0x1c] = 0xf000ff53; /* F000:FF53 (User Timer Hook IRET) */

    /* Install default stubs in Shadow RAM F0000 */
    uint8_t *f0000_ptr = (uint8_t *)memory_region_get_ram_ptr(shadow_ram_f0000);
    /* F000:FF53 -> iret */
    f0000_ptr[0xff53] = 0xcf;
    /* F000:FF40 -> push %ax; mov $0x20, %al; out %al, $0x20; pop %ax; iret */
    f0000_ptr[0xff40] = 0x50;
    f0000_ptr[0xff41] = 0xb0;
    f0000_ptr[0xff42] = 0x20;
    f0000_ptr[0xff43] = 0xe6;
    f0000_ptr[0xff44] = 0x20;
    f0000_ptr[0xff45] = 0x58;
    f0000_ptr[0xff46] = 0xcf;
    /* F000:FF30 -> push %ax; mov $0x20, %al; out %al, $0xa0; out %al, $0x20; pop %ax; iret */
    f0000_ptr[0xff30] = 0x50;
    f0000_ptr[0xff31] = 0xb0;
    f0000_ptr[0xff32] = 0x20;
    f0000_ptr[0xff33] = 0xe6;
    f0000_ptr[0xff34] = 0xa0;
    f0000_ptr[0xff35] = 0xe6;
    f0000_ptr[0xff36] = 0x20;
    f0000_ptr[0xff37] = 0x58;
    f0000_ptr[0xff38] = 0xcf;

    /* Install INT 10h ROM Stub at 0xF000:F000: mov $0x0180, %dx; out %al, (%dx); iret */
    f0000_ptr[0xf000] = 0xba;
    f0000_ptr[0xf001] = 0x80;
    f0000_ptr[0xf002] = 0x01;
    f0000_ptr[0xf003] = 0xee;
    f0000_ptr[0xf004] = 0xcf;

    /* Place safety IRET at 0x00000000 so any jumps to 0x0000:0000 return cleanly */
    *(uint8_t *)memory_region_get_ram_ptr(machine->ram) = 0xcf;

    /* Initialize BDA Video Variables (0x0040:0x0049..0x0089) for VGA Color Display */
    toshiba_system_ram = (uint8_t *)memory_region_get_ram_ptr(machine->ram);
    uint8_t *ram = toshiba_system_ram;
    *(uint16_t *)(ram + 0x410) = 0x0021; /* Equipment: 80x25 color display, floppy present */
    ram[0x449] = 0x03;                   /* Video mode 03h (80x25 color text) */
    *(uint16_t *)(ram + 0x44a) = 80;     /* 80 columns */
    *(uint16_t *)(ram + 0x44c) = 0x1000; /* 4KB page size */
    *(uint16_t *)(ram + 0x463) = 0x03d4; /* CRTC base I/O port */
    ram[0x484] = 24;                     /* 25 rows (24 + 1) */
    *(uint16_t *)(ram + 0x485) = 16;     /* 16-pixel character font height */
    ram[0x487] = 0x60;                   /* EGA/VGA active display combination */
    ram[0x488] = 0x09;                   /* Video feature control */
    ram[0x489] = 0x51;                   /* Video display flags */

    /* Toshiba Portege M200 Hardware Straps (Port 0x85: 0x08 = Normal Boot SXGA+ Panel) */
    MemoryRegion *strap_io = g_new(MemoryRegion, 1);
    memory_region_init_io(strap_io, OBJECT(machine), &toshiba_m200_io_ops, NULL,
                          "m200-strap-io", 1);
    memory_region_add_subregion(system_io, 0x85, strap_io);

    /* Toshiba Embedded Controller (Ports 0xEE/0xEF) */
    ToshibaECState *ec = g_new0(ToshibaECState, 1);
    MemoryRegion *ec_io = g_new(MemoryRegion, 1);
    memory_region_init_io(ec_io, OBJECT(machine), &toshiba_ec_ops, ec, "toshiba-ec", 2);
    memory_region_add_subregion(system_io, 0xee, ec_io);

    /* ACPI Embedded Controller (Ports 0x62 Data / 0x66 Status & Command) */
    ToshibaAcpiECState *acpi_ec = g_new0(ToshibaAcpiECState, 1);
    MemoryRegion *acpi_ec_data = g_new(MemoryRegion, 1);
    MemoryRegion *acpi_ec_cmd = g_new(MemoryRegion, 1);
    memory_region_init_io(acpi_ec_data, OBJECT(machine), &toshiba_acpi_ec_data_ops, acpi_ec, "acpi-ec-data", 1);
    memory_region_init_io(acpi_ec_cmd, OBJECT(machine), &toshiba_acpi_ec_cmd_ops, acpi_ec, "acpi-ec-cmd", 1);
    memory_region_add_subregion(system_io, 0x62, acpi_ec_data);
    memory_region_add_subregion(system_io, 0x66, acpi_ec_cmd);

    /* Toshiba Hardware Debug Port (PJ2000 on Ports 0x1E0..0x1E7) */
    ToshibaDPortState *dport = g_new0(ToshibaDPortState, 1);
    MemoryRegion *dport_io = g_new(MemoryRegion, 1);
    memory_region_init_io(dport_io, OBJECT(machine), &toshiba_dport_ops, dport, "toshiba-dport", 8);
    memory_region_add_subregion(system_io, 0x1e0, dport_io);

    /* Intel ICH4-M ACPI Power Management Timer (Base 0xD800, Timer at 0xD808) */
    MemoryRegion *acpi_pm = g_new(MemoryRegion, 1);
    memory_region_init_io(acpi_pm, OBJECT(machine), &toshiba_acpi_pm_ops, NULL, "m200-acpi-pm", 0x40);
    memory_region_add_subregion_overlap(system_io, 0xd800, acpi_pm, 100);

    /* Port 0x64 KBC Status (SYSF set) */
    MemoryRegion *kbc_status_io = g_new(MemoryRegion, 1);
    memory_region_init_io(kbc_status_io, OBJECT(machine), &toshiba_kbc_status_ops, NULL, "m200-kbc-status", 1);
    memory_region_add_subregion_overlap(system_io, 0x64, kbc_status_io, 50);

    /* Intel ICH4-M SMBus Controller (Base 0xD880) */
    ToshibaSMBusState *smbus = g_new0(ToshibaSMBusState, 1);
    MemoryRegion *smbus_io = g_new(MemoryRegion, 1);
    memory_region_init_io(smbus_io, OBJECT(machine), &toshiba_smbus_ops, smbus, "m200-smbus", 0x20);
    memory_region_add_subregion_overlap(system_io, 0xd880, smbus_io, 100);

    /* Toshiba NoteBIOS High SMRAM / TSEG Window (0xFED00000..0xFEDFFFFF, size 1MB) */
    MemoryRegion *smram_tseg = g_new(MemoryRegion, 1);
    memory_region_init_ram(smram_tseg, NULL, "m200-smram-tseg", 0x100000, &error_fatal);
    memory_region_add_subregion(system_memory, 0xfed00000, smram_tseg);
}

static void m200_machine_options(MachineClass *m)
{
    PCMachineClass *pcmc = PC_MACHINE_CLASS(m);

    pcmc->default_south_bridge = NULL; /* Using ICH4M */
    pcmc->pci_root_uid = 0;
    pcmc->default_cpu_version = 1;
    pcmc->pci_enabled = true;

    m->family = "portege_m200";
    m->desc = "Toshiba Portege M200 (i855PM + ICH4M)";
    m->default_machine_opts = "firmware=bios-256k.bin";
    m->default_display = "std";
    m->default_nic = "e1000";
    m->default_cpu_type = X86_CPU_TYPE_NAME("pentium-m");
    m->no_floppy = 1;
    m->max_cpus = 1; /* Single-core Pentium M */
}

static void m200_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->init = m200_init;
    mc->default_ram_size = 512 * MiB;
    mc->default_ram_id = "portege_m200.ram";

    m200_machine_options(mc);
}

static const TypeInfo m200_machine_info = {
    .name = MACHINE_TYPE_NAME("m200"),
    .parent = TYPE_PC_MACHINE,
    .class_init = m200_machine_class_init,
    .interfaces = (const InterfaceInfo[]) {
        { TYPE_MACHINE },
        { }
    }
};

static void m200_machine_register_types(void)
{
    type_register_static(&m200_machine_info);
}

type_init(m200_machine_register_types);


