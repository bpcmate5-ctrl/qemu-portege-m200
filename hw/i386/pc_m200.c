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
#include "system/reset.h"
#include "exec/tb-flush.h"
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
#include "hw/i2c/smbus_master.h"
#include "hw/i2c/m200_smbus.h"
#include "hw/southbridge/ich9.h"
#include "hw/display/nv34m.h"
#include "hw/i386/m200_ec.h"
#include "hw/char/serial-isa.h"
#include "chardev/char.h"
#include "system/hw_accel.h"
#include "qemu/datadir.h"
#include "qemu/timer.h"
#include "hw/core/loader.h"

/* Toshiba Portege M200 Motherboard Straps & Hardware ID (Port 0x85) */
static uint8_t toshiba_m200_port85 = 0x00;
static uint8_t *toshiba_system_ram = NULL;
static MemoryRegion *g_shadow_ram_f0000 = NULL;
static MemoryRegion *g_high_smram = NULL;

uint32_t pc_m200_get_cpu_eip(void)
{
    CPUState *cpu = current_cpu ? current_cpu : first_cpu;
    if (cpu) {
        X86CPU *x86_cpu = X86_CPU(cpu);
        return x86_cpu->env.eip;
    }
    return 0;
}

void *pc_m200_get_high_smram_ptr(void)
{
    if (g_high_smram) {
        return memory_region_get_ram_ptr(g_high_smram);
    }
    return NULL;
}

/* Compatibility for the existing passive M200 translator instrumentation. */
void *pc_m200_get_tseg_ptr(void)
{
    return pc_m200_get_high_smram_ptr();
}

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

/* Toshiba Portege M200 Hardware Debug Port (PJ2000 - MS Debug Port Spec v1.0 on 0x1E0..0x1E7) */
typedef struct ToshibaDPortState {
    uint8_t byte_count;
    uint16_t current_code;
    uint64_t sequence;
} ToshibaDPortState;

static const char *dport_code_name(uint16_t code)
{
    switch (code) {
    case 0xF000: return "Early init (A20, PIT CH0, BootBlock Checksum)";
    case 0xF001: return "EC/KBC check";
    case 0xF002: return "Security controller initialization";
    case 0xF003: return "BIOS rewrite request processing";
    case 0xF004: return "BIOS rewrite request processing";
    case 0xF005: return "BIOS rewrite request processing";
    case 0xF006: return "BIOS signature check";
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
    case 0xFFFF: return "NORMAL D-port STATUS";
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
            uint8_t byte = val & 0xff;

            /* PJ2000 checkpoint records are class byte F0/F1/FF plus code. */
            if (byte == 0xf0 || byte == 0xf1 || byte == 0xff) {
                dp->current_code = byte;
                dp->byte_count = 1;
            }
        } else {
            dp->current_code = (dp->current_code << 8) | (val & 0xff);
            dp->byte_count = 0;
            CPUState *cpu = first_cpu;
            if (cpu) {
                X86CPU *x86_cpu = X86_CPU(cpu);
                CPUX86State *env;

                cpu_synchronize_state(cpu);
                env = &x86_cpu->env;
                fprintf(stderr,
                        "[M200-CHK #%04" PRIu64 "] %04X "
                        "CS:EIP=%04x:%08x phys=%08x "
                        "EAX=%08x EBX=%08x ECX=%08x EDX=%08x "
                        "ESI=%08x EDI=%08x ESP=%08x EBP=%08x "
                        "DI=%04x BH=%02x BL=%02x "
                        "CS=%04x SS=%04x DS=%04x ES=%04x FS=%04x GS=%04x "
                        "CR0=%08x EFLAGS=%08x\n",
                        ++dp->sequence, dp->current_code,
                        env->segs[R_CS].selector, (uint32_t)env->eip,
                        (uint32_t)(env->segs[R_CS].base + env->eip),
                        (uint32_t)env->regs[R_EAX],
                        (uint32_t)env->regs[R_EBX],
                        (uint32_t)env->regs[R_ECX],
                        (uint32_t)env->regs[R_EDX],
                        (uint32_t)env->regs[R_ESI],
                        (uint32_t)env->regs[R_EDI],
                        (uint32_t)env->regs[R_ESP],
                        (uint32_t)env->regs[R_EBP],
                        (uint16_t)env->regs[R_EDI],
                        (uint8_t)(env->regs[R_EBX] >> 8),
                        (uint8_t)env->regs[R_EBX],
                        env->segs[R_CS].selector, env->segs[R_SS].selector,
                        env->segs[R_DS].selector, env->segs[R_ES].selector,
                        env->segs[R_FS].selector, env->segs[R_GS].selector,
                        (uint32_t)env->cr[0], (uint32_t)env->eflags);
            }
            if (dp->current_code == 0xF113) {
                fprintf(stderr, "\n============================================================\n"
                                "   ENTERING F113: DISP_LOGO (Authentic Toshiba Splash Mode)\n"
                                "   EIP=0x%08x\n"
                                "============================================================\n",
                        pc_m200_get_cpu_eip());
            } else {
                fprintf(stderr, "\n>>> [TOSHIBA D-PORT PJ2000] 0x%04X : %s <<<\n",
                        dp->current_code, dport_code_name(dp->current_code));
            }
        }
    }
}

static const MemoryRegionOps toshiba_dport_ops = {
    .read = toshiba_dport_read,
    .write = toshiba_dport_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
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

/* Real VBIOS INT 10h Mode Toggle (false = compatibility C shim, true = authentic VBIOS IVT) */
static bool m200_real_vbios_int10 = true;

/* IVT Memory Intercept (0x00000000..0x000003FF) */
static uint64_t toshiba_ivt_read(void *opaque, hwaddr addr, unsigned size)
{
    if (addr == 0x40 && size == 4) {
        if (m200_real_vbios_int10 && toshiba_system_ram) {
            uint32_t vec = *(uint32_t *)(toshiba_system_ram + 0x40);
            return vec;
        }
        return 0xf000f000;
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
    if (addr >= 0x40 && addr < 0x44) {
        uint32_t old_vec = toshiba_system_ram ? *(uint32_t *)(toshiba_system_ram + 0x40) : 0xf000f000;
        uint32_t new_vec = old_vec;
        memcpy((uint8_t *)&new_vec + (addr - 0x40), &val, size);
        fprintf(stderr, "[M200 INT10 IVT WRITE] offset=0x%02x val=0x%08"PRIx64" size=%u (old=%04x:%04x, new=%04x:%04x, eip=0x%08x)\n",
                (uint32_t)addr, val, size,
                (old_vec >> 16) & 0xffff, old_vec & 0xffff,
                (new_vec >> 16) & 0xffff, new_vec & 0xffff,
                pc_m200_get_cpu_eip());

        if (m200_real_vbios_int10 && toshiba_system_ram) {
            memcpy(toshiba_system_ram + addr, &val, size);
        }
        /* In compatibility mode: keep C trap active without modifying underlying IVT */
        return;
    }
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

static void toshiba_port80_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    int64_t t_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    fprintf(stderr, "[M200 POST CODE 0x80] t=%8.3f ms | code=0x%02x (eip=0x%08x)\n",
            t_ns / 1000000.0, (uint8_t)val, pc_m200_get_cpu_eip());
}

static uint64_t toshiba_port80_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0xff;
}

static const MemoryRegionOps toshiba_port80_ops = {
    .read = toshiba_port80_read,
    .write = toshiba_port80_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
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
            uint8_t c = (al >= 2) ? toshiba_system_ram[str_phys + k * 2] : toshiba_system_ram[str_phys + k];
            uint8_t at = (al >= 2) ? toshiba_system_ram[str_phys + k * 2 + 1] : (bl ? bl : 0x07);
            toshiba_vga_set_char(dh, dl + k, c, at);
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
    ICH4MLPCState *lpc;
    I2CBus *bus;
    QEMUTimer *transaction_timer;
    uint8_t hst_sts;
    uint8_t hst_cnt;
    uint8_t hst_cmd;
    uint8_t xmit_slva;
    uint8_t hst_d0;
    uint8_t hst_d1;
    uint8_t block_data;
    uint8_t active_cnt;
    uint8_t active_cmd;
    uint8_t active_slva;
    uint8_t active_d0;
    uint8_t active_d1;
    uint8_t block_data_bytes[32];
    uint8_t active_block_data[32];
    uint8_t block_index;
    uint8_t active_block_len;
    bool transaction_pending;
    bool busy_observed;
    bool trace;
    int64_t scheduled_ns;
} ToshibaSMBusState;

#define M200_SMB_STS_HOST_BUSY 0x01
#define M200_SMB_STS_INTR      0x02
#define M200_SMB_STS_DEV_ERR   0x04
#define M200_SMB_STS_BUS_ERR   0x08
#define M200_SMB_STS_FAILED    0x10
#define M200_SMB_CNT_INTREN     0x01
#define M200_SMB_CNT_KILL       0x02
#define M200_SMB_CNT_START      0x40

/*
 * ICH4-M is an SMBus 2.0 host.  Model one standard-mode clock as 10 us.
 * An unclaimed address completes after START hold, the address byte plus
 * ACK/NACK clock, and STOP setup: 4 + 9*10 + 4 = 98 us.  This is distinct
 * from the documented 25--35 ms clock-held/device timeout.
 */
#define M200_SMBUS_CLOCK_NS       10000ULL
#define M200_SMBUS_START_HOLD_NS   4000ULL
#define M200_SMBUS_STOP_SETUP_NS   4000ULL

static const uint8_t ddr_spd_dimm0[128];

static uint64_t m200_smbus_wire_time_ns(unsigned bytes)
{
    return M200_SMBUS_START_HOLD_NS +
           bytes * 9 * M200_SMBUS_CLOCK_NS +
           M200_SMBUS_STOP_SETUP_NS;
}

static void m200_smbus_transaction_complete(void *opaque)
{
    ToshibaSMBusState *s = opaque;
    uint8_t dev_addr = s->active_slva >> 1;
    int64_t now_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    if (!s->transaction_pending) {
        return;
    }
    s->transaction_pending = false;
    s->hst_sts &= ~M200_SMB_STS_HOST_BUSY;

    int result = -1;
    unsigned protocol = (s->active_cnt >> 2) & 7;
    bool read = s->active_slva & 1;

    switch (protocol) {
    case 0: /* Quick */
        result = smbus_quick_command(s->bus, dev_addr, read);
        break;
    case 1: /* Byte */
        result = read ? smbus_receive_byte(s->bus, dev_addr) :
                        smbus_send_byte(s->bus, dev_addr, s->active_cmd);
        if (read && result >= 0) {
            s->hst_d0 = result;
        }
        break;
    case 2: /* Byte data */
        result = read ? smbus_read_byte(s->bus, dev_addr, s->active_cmd) :
                        smbus_write_byte(s->bus, dev_addr, s->active_cmd,
                                         s->active_d0);
        if (read && result >= 0) {
            s->hst_d0 = result;
        }
        break;
    case 3: /* Word data */
        result = read ? smbus_read_word(s->bus, dev_addr, s->active_cmd) :
                        smbus_write_word(s->bus, dev_addr, s->active_cmd,
                                         s->active_d0 |
                                         ((uint16_t)s->active_d1 << 8));
        if (read && result >= 0) {
            s->hst_d0 = result;
            s->hst_d1 = result >> 8;
        }
        break;
    case 5: /* Block */
        if (!read) {
            result = smbus_write_block(s->bus, dev_addr, s->active_cmd,
                                       s->active_block_data,
                                       s->active_block_len, true);
        }
        break;
    default:
        break;
    }

    if (result >= 0) {
        s->hst_sts |= M200_SMB_STS_INTR;
    } else {
        /* Address NACK is reported as DEV_ERR, not as a bus timeout. */
        s->hst_sts |= M200_SMB_STS_DEV_ERR;
    }
    if (getenv("M200_ICOUNT_TIMER_TRACE")) {
        fprintf(stderr,
                "[M200-ICOUNT-TIMER] SMBUS slva=%02x scheduled=%" PRId64
                " actual=%" PRId64 " late=%" PRId64 "\n",
                s->active_slva, s->scheduled_ns, now_ns,
                now_ns - s->scheduled_ns);
    }
}

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

    if (!ich4m_lpc_smbus_for_bios_enabled(s->lpc)) {
        return UINT64_MAX;
    }
    switch (addr) {
    case 0x00: /* HST_STS */
        val = s->hst_sts;
        if (val & M200_SMB_STS_HOST_BUSY) {
            s->busy_observed = true;
        }
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
    case 0x06: /* HST_D1 */
        val = s->hst_d1;
        break;
    case 0x07: /* HOST_BLOCK_DB */
        val = s->block_data;
        break;
    default:
        val = 0;
        break;
    }
    if (s->trace) {
        fprintf(stderr, ">>> [TOSHIBA SMBUS READ] Port 0x%04X -> 0x%02X\n",
                (unsigned)(0xD880 + addr), val);
    }
    return val;
}

static void toshiba_smbus_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    ToshibaSMBusState *s = opaque;

    if (!ich4m_lpc_smbus_for_bios_enabled(s->lpc)) {
        return;
    }
    if (s->trace) {
        fprintf(stderr, ">>> [TOSHIBA SMBUS WRITE] Port 0x%04X <- 0x%02X\n",
                (unsigned)(0xD880 + addr), (uint8_t)val);
    }
    switch (addr) {
    case 0x00: /* HST_STS (W1C) */
        s->hst_sts &= ~(val & ~M200_SMB_STS_HOST_BUSY & 0xff);
        break;
    case 0x02: /* HST_CNT */
        s->hst_cnt = val & ~M200_SMB_CNT_START & 0xff;
        if (val & M200_SMB_CNT_KILL) {
            if (s->transaction_pending) {
                timer_del(s->transaction_timer);
                s->transaction_pending = false;
                s->hst_sts &= ~M200_SMB_STS_HOST_BUSY;
                s->hst_sts |= M200_SMB_STS_FAILED;
            }
        } else if ((val & M200_SMB_CNT_START) &&
                   !(s->hst_sts & M200_SMB_STS_HOST_BUSY)) {
            unsigned wire_bytes = 1;

            s->active_cnt = val;
            s->active_cmd = s->hst_cmd;
            s->active_slva = s->xmit_slva;
            s->active_d0 = s->hst_d0;
            s->active_d1 = s->hst_d1;
            s->active_block_len = MIN(s->hst_d0, (uint8_t)32);
            memcpy(s->active_block_data, s->block_data_bytes,
                   s->active_block_len);
            s->transaction_pending = true;
            s->busy_observed = false;
            s->hst_sts |= M200_SMB_STS_HOST_BUSY;
            switch ((val >> 2) & 7) {
            case 0: wire_bytes = 1; break; /* quick */
            case 1: wire_bytes = 2; break; /* byte */
            case 2: wire_bytes = 3; break; /* byte data */
            case 3: wire_bytes = 4; break; /* word data */
            case 5: wire_bytes = 3 + MIN(s->hst_d0, 32); break;
            default: wire_bytes = 4; break;
            }
            s->scheduled_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                              m200_smbus_wire_time_ns(wire_bytes);
            timer_mod(s->transaction_timer, s->scheduled_ns);
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
        s->block_index = 0;
        break;
    case 0x06: /* HST_D1 */
        s->hst_d1 = val & 0xff;
        break;
    case 0x07: /* HOST_BLOCK_DB */
        s->block_data = val & 0xff;
        if (s->block_index < ARRAY_SIZE(s->block_data_bytes)) {
            s->block_data_bytes[s->block_index++] = val;
        }
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

static MemoryRegion *g_shadow_c0000 = NULL;
static MemoryRegion *g_shadow_e0000 = NULL;
static MemoryRegion *g_shadow_f0000 = NULL;
static uint8_t g_bios_image[0x80000];
static bool g_bios_loaded = false;

static void pc_m200_reset_handler(void *opaque)
{
    X86MachineState *x86ms = opaque;
    if (x86ms->bios.ram_block) {
        uint8_t *bios_ptr = memory_region_get_ram_ptr(&x86ms->bios);
        int bios_size = memory_region_size(&x86ms->bios);
        if (g_bios_loaded && bios_size >= 0x80000) {
            memcpy(bios_ptr, g_bios_image, 0x80000);
        }
        if (bios_size >= 0x10) {
            uint8_t *rv = bios_ptr + bios_size - 0x10;
            rv[0] = 0xea;
            rv[1] = 0x1c;
            rv[2] = 0x29;
            rv[3] = 0x00;
            rv[4] = 0xf0; /* ljmp $0xF000, $0x291C */
        }
        uint8_t buf[16];
        cpu_physical_memory_read(0xfffffff0, buf, 16);
        fprintf(stderr, "[RESET HANDLER] 0xFFFFFFF0: %02x %02x %02x %02x %02x\n",
                buf[0], buf[1], buf[2], buf[3], buf[4]);
        cpu_physical_memory_read(0x000ffff0, buf, 16);
        fprintf(stderr, "[RESET HANDLER] 0x000FFFF0: %02x %02x %02x %02x %02x\n",
                buf[0], buf[1], buf[2], buf[3], buf[4]);
        if (g_bios_loaded) {
            if (g_shadow_f0000) {
                uint8_t *f0000_ptr = (uint8_t *)memory_region_get_ram_ptr(g_shadow_f0000);
                memcpy(f0000_ptr, g_bios_image + 0x20000, 0x10000);
                f0000_ptr[0xfff0] = 0xea;
                f0000_ptr[0xfff1] = 0x1c;
                f0000_ptr[0xfff2] = 0x29;
                f0000_ptr[0xfff3] = 0x00;
                f0000_ptr[0xfff4] = 0xf0;
            }
            if (g_shadow_e0000) {
                uint8_t *e0000_ptr = (uint8_t *)memory_region_get_ram_ptr(g_shadow_e0000);
                memcpy(e0000_ptr, g_bios_image + 0x30000, 0x10000);
                /* Module 3 shadow entry headers */
                e0000_ptr[0] = 0xe9;
                e0000_ptr[1] = 0xb8;
                e0000_ptr[2] = 0x69; /* jmp 0x69bb */
                e0000_ptr[3] = 0xcb; /* lret */
                e0000_ptr[4] = 0xe9;
                e0000_ptr[5] = 0x8d;
                e0000_ptr[6] = 0xeb; /* jmp 0xeb94 */
                e0000_ptr[0x6ca1] = 0xcb; /* lret to caller */
            }
            if (g_shadow_c0000) {
                uint8_t *c0000_ptr = (uint8_t *)memory_region_get_ram_ptr(g_shadow_c0000);
                memcpy(c0000_ptr, g_bios_image + 0x40000, 0x10000);
            }
        }
        if (g_high_smram && g_bios_loaded) {
            uint8_t *high_ptr = memory_region_get_ram_ptr(g_high_smram);
            uint8_t *mod3 = g_bios_image + 0x30000;
            memcpy(high_ptr, mod3, 0x10000);
            memset(high_ptr + 0x10000, 0, 0x10000);

            uint8_t *gdt_dest = high_ptr + 0x1f000;
            memcpy(gdt_dest, mod3 + 0x0046, 0x130);
            high_ptr[0x19ef0] = 0x04;
            high_ptr[0x139df] = 0x0c;
            high_ptr[0x139e6] = 0x40;
            high_ptr[0x139ad] = 0x80;
            high_ptr[0x139ae] = 0x80;
            high_ptr[0x139af] = 0x01;
            high_ptr[0x1a140] = 0x08;
            high_ptr[0x19fbf] = 0x40;
            gdt_dest[0x32] = 0x00; gdt_dest[0x34] = 0xda; gdt_dest[0x37] = 0xfe;
            gdt_dest[0x3a] = 0x00; gdt_dest[0x3c] = 0xda; gdt_dest[0x3f] = 0xfe;

            uint8_t *idt = high_ptr + 0x1f140;
            idt[0] = 0x53; idt[1] = 0xff;
            memcpy(idt + 2, mod3 + 0x772f, 6);
            for (int k = 1; k < 128; k++) {
                memcpy(idt + k * 8, idt, 8);
            }
            for (int p = 0x7735; p < 0x7771; p += 3) {
                uint8_t int_num = mod3[p];
                uint16_t target_offset = *(uint16_t *)(mod3 + p + 1);
                uint16_t *gate = (uint16_t *)(idt + int_num * 8);
                gate[0] = target_offset;
            }
            if (first_cpu) {
                queue_tb_flush(first_cpu);
            }
        }
    }
}

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

    I855PMHost *i855 = I855PM_HOST_DEVICE(phb);
    g_high_smram = &i855->mch.high_smram_backing;

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

    /* Initialize CMOS NVRAM parameters for RTC (0x70/0x71) */
    MC146818RtcState *rtc_s = MC146818_RTC(x86ms->rtc);
    if (rtc_s) {
        /* Base memory: 640 KB */
        mc146818rtc_set_cmos_data(rtc_s, 0x15, 640 & 0xff);
        mc146818rtc_set_cmos_data(rtc_s, 0x16, 640 >> 8);
        /* Extended memory: 511 MB */
        int ext_mem = (512 * 1024 - 1024); /* in KB */
        if (ext_mem > 65535) ext_mem = 65535;
        mc146818rtc_set_cmos_data(rtc_s, 0x17, ext_mem & 0xff);
        mc146818rtc_set_cmos_data(rtc_s, 0x18, ext_mem >> 8);
        mc146818rtc_set_cmos_data(rtc_s, 0x30, ext_mem & 0xff);
        mc146818rtc_set_cmos_data(rtc_s, 0x31, ext_mem >> 8);
        /* Memory above 16MB: 496 MB (in 64KB blocks -> 7936 = 0x1F00) */
        mc146818rtc_set_cmos_data(rtc_s, 0x34, 0x00);
        mc146818rtc_set_cmos_data(rtc_s, 0x35, 0x1f);
        /* Hard drive type: Primary Master present (0xF0) */
        mc146818rtc_set_cmos_data(rtc_s, 0x12, 0xf0);
        /* Equipment byte: 80x25 color, 1 FDD, FPU */
        mc146818rtc_set_cmos_data(rtc_s, 0x14, 0x27);
        /* Boot sequence: CD-ROM first (0x03), then HDD (0x02), then FDD (0x01) */
        mc146818rtc_set_cmos_data(rtc_s, 0x38, 0x32);
        mc146818rtc_set_cmos_data(rtc_s, 0x3d, 0x20);
        /* Shutdown status byte: 0x00 = Initial Cold Power-on State */
        mc146818rtc_set_cmos_data(rtc_s, 0x0f, 0x00);
    }

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
        ich4m_lpc_set_smbus_device(ICH4M_LPC_DEVICE(lpc), smb);
        pcms->smbus = I2C_BUS(qdev_get_child_bus(DEVICE(smb), "i2c"));
        smbus_eeprom_init(pcms->smbus, 8, ddr_spd_dimm0,
                          sizeof(ddr_spd_dimm0));
        m200_smbus_create_board_devices(pcms->smbus);
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

        /* AC'97 Modem (D31:F6) */
        PCIDevice *modem = pci_create_simple_multifunction(pcms->pcibus,
                                                          PCI_DEVFN(ICH4M_AC97_DEV, 6),
                                                          "AC97");
        pci_config_set_vendor_id(modem->config, PCI_VENDOR_ID_INTEL);
        pci_config_set_device_id(modem->config, PCI_DEVICE_ID_INTEL_82801DBM_MODEM);
        pci_config_set_revision(modem->config, 0x03);
        pci_set_word(modem->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
        pci_set_word(modem->config + PCI_SUBSYSTEM_ID, 0x0001);
    }

    /* Video (NVIDIA GeForce FX Go5200 / NV34M on AGP Bridge 00:01.0 -> 01:00.0) & Network */
    PCIBus *agp_bus = pci_bridge_get_sec_bus(PCI_BRIDGE(&i855->agp));
    pci_create_simple(agp_bus, PCI_DEVFN(0, 0), TYPE_NV34M_VGA);
    pc_nic_init(pcmc, isa_bus, pcms->pcibus);

    /* Map POST Checkpoint Port 0x80 */
    MemoryRegion *port80_io = g_new(MemoryRegion, 1);
    memory_region_init_io(port80_io, OBJECT(machine), &toshiba_port80_ops, NULL,
                          "toshiba-port80", 1);
    memory_region_add_subregion_overlap(system_io, 0x80, port80_io, 100);

    /* Map Toshiba authentic shadow RAM when 512K ROM is loaded */
    if (machine->firmware && (strstr(machine->firmware, "512") || strstr(machine->firmware, "TOSHIBA"))) {
        MemoryRegion *shadow_ram_c0000 = g_new(MemoryRegion, 1);
        memory_region_init_ram(shadow_ram_c0000, NULL, "m200.shadow-c0000", 0x10000, &error_fatal);
        MemoryRegion *shadow_ram_e0000 = g_new(MemoryRegion, 1);
        memory_region_init_ram(shadow_ram_e0000, NULL, "m200.shadow-e0000", 0x10000, &error_fatal);
        MemoryRegion *shadow_ram_f0000 = g_new(MemoryRegion, 1);
        memory_region_init_ram(shadow_ram_f0000, NULL, "m200.shadow-f0000", 0x10000, &error_fatal);
        g_shadow_c0000 = shadow_ram_c0000;
        g_shadow_e0000 = shadow_ram_e0000;
        g_shadow_f0000 = shadow_ram_f0000;
        g_shadow_ram_f0000 = shadow_ram_f0000;

        char *filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, machine->firmware);
        if (filename) {
            FILE *f = fopen(filename, "rb");
            if (f) {
                uint8_t *ram = (uint8_t *)memory_region_get_ram_ptr(machine->ram);
                /* Preload full 512KB ROM into low RAM and static cache */
                fseek(f, 0, SEEK_SET);
                if (fread(ram, 1, 0x80000, f) != 0x80000) {}
                fseek(f, 0, SEEK_SET);
                if (fread(g_bios_image, 1, 0x80000, f) == 0x80000) {
                    g_bios_loaded = true;
                }

                /* Preload Module 3 into High SMRAM at 0xFEDA0000 and clear BSS at 0xFEDB0000 */
                uint8_t *high_ptr = memory_region_get_ram_ptr(g_high_smram);
                fseek(f, 0x30000, SEEK_SET);
                if (fread(high_ptr, 1, 0x10000, f) == 0x10000) {}
                memset(high_ptr + 0x10000, 0, 0x10000);

                uint8_t *gdt_dest = high_ptr + 0x1f000;
                uint8_t *mod3 = high_ptr;
                memcpy(gdt_dest, mod3 + 0x0046, 0x130);
                high_ptr[0x19ef0] = 0x04;
                high_ptr[0x139df] = 0x0c;
                high_ptr[0x139e6] = 0x40;
                high_ptr[0x139ad] = 0x80;
                high_ptr[0x139ae] = 0x80;
                high_ptr[0x139af] = 0x01;
                high_ptr[0x1a140] = 0x08;
                high_ptr[0x19fbf] = 0x40;
                /* Pre-configure Selector 0x30 and 0x38 to point to 0xFEDA0000 */
                gdt_dest[0x32] = 0x00; gdt_dest[0x34] = 0xda; gdt_dest[0x37] = 0xfe;
                gdt_dest[0x3a] = 0x00; gdt_dest[0x3c] = 0xda; gdt_dest[0x3f] = 0xfe;

                /* Preload authentic NoteBIOS IDT at FEDBF140. */
                uint8_t *idt = high_ptr + 0x1f140;
                idt[0] = 0x53; idt[1] = 0xff;
                memcpy(idt + 2, mod3 + 0x772f, 6);
                for (int k = 1; k < 128; k++) {
                    memcpy(idt + k * 8, idt, 8);
                }
                for (int p = 0x7735; p < 0x7771; p += 3) {
                    uint8_t int_num = mod3[p];
                    uint16_t target_offset = *(uint16_t *)(mod3 + p + 1);
                    uint16_t *gate = (uint16_t *)(idt + int_num * 8);
                    gate[0] = target_offset;
                }

                /* Preload Module 4 Authentic NVIDIA VBIOS at 0xC0000 */
                fseek(f, 0x40000, SEEK_SET);
                if (fread(memory_region_get_ram_ptr(shadow_ram_c0000), 1, 0x10000, f) == 0x10000) {}

                /* Preload Module 3 POST Graphics at 0xE0000 */
                fseek(f, 0x30000, SEEK_SET);
                if (fread(memory_region_get_ram_ptr(shadow_ram_e0000), 1, 0x10000, f) == 0x10000) {
                    uint8_t *e0000_ptr = (uint8_t *)memory_region_get_ram_ptr(shadow_ram_e0000);
                    uint8_t sum = 0;
                    for (int k = 0; k < 0x8000; k++) {
                        sum += e0000_ptr[k];
                    }
                    e0000_ptr[0x7fff] -= sum;
                    /* Module 3 shadow entry headers */
                    e0000_ptr[0] = 0xe9;
                    e0000_ptr[1] = 0xb8;
                    e0000_ptr[2] = 0x69; /* jmp 0x69bb */
                    e0000_ptr[3] = 0xcb; /* lret */
                    e0000_ptr[4] = 0xe9;
                    e0000_ptr[5] = 0x8d;
                    e0000_ptr[6] = 0xeb; /* jmp 0xeb94 */
                    e0000_ptr[0x6ca1] = 0xcb; /* lret to caller */
                }

                /* Preload Module 2 System BIOS into 0xF0000 */
                fseek(f, 0x20000, SEEK_SET);
                if (fread(memory_region_get_ram_ptr(shadow_ram_f0000), 1, 0x10000, f) == 0x10000) {}

                /* Place authentic System BIOS entry jump at 0x000FFFF0 and 0xFFFFFFF0 -> ljmp $0xF000, $0x291C */
                uint8_t *ptr = (uint8_t *)memory_region_get_ram_ptr(shadow_ram_f0000);
                ptr[0xfff0] = 0xea;
                ptr[0xfff1] = 0x1c;
                ptr[0xfff2] = 0x29;
                ptr[0xfff3] = 0x00;
                ptr[0xfff4] = 0xf0; /* ljmp $0xF000, $0x291C (Authentic Module 2 Cold POST Entry) */

                fclose(f);
            }
            g_free(filename);
        }
        memory_region_add_subregion_overlap(system_memory, 0xc0000, shadow_ram_c0000, 1000);
        memory_region_add_subregion_overlap(system_memory, 0xe0000, shadow_ram_e0000, 1000);
        memory_region_add_subregion_overlap(system_memory, 0xf0000, shadow_ram_f0000, 1000);
    }

    /* Map Video BIOS INT 10h Service Port (0x180) */
    MemoryRegion *vga_int10_io = g_new(MemoryRegion, 1);
    memory_region_init_io(vga_int10_io, OBJECT(machine), &toshiba_vga_int10_ops, NULL,
                          "m200-vga-int10", 2);
    memory_region_add_subregion_overlap(system_io, 0x180, vga_int10_io, 100);

    /* Map Toshiba Hardware Debug Port (PJ2000 on Ports 0x1E0..0x1E7) */
    ToshibaDPortState *dport = g_new0(ToshibaDPortState, 1);
    MemoryRegion *sio_1e0_mr = g_new(MemoryRegion, 1);
    memory_region_init_io(sio_1e0_mr, OBJECT(machine), &toshiba_dport_ops, dport,
                          "toshiba-dport", 8);
    memory_region_add_subregion_overlap(system_io, 0x1e0, sio_1e0_mr, 100);

    /* Initialize BDA (0x00000400..0x000004FF) and System Parameters */
    uint8_t *ram_base = (uint8_t *)memory_region_get_ram_ptr(machine->ram);
    *(uint32_t *)(ram_base + 0x40) = 0xf000f000; /* INT 10h Video Vector -> 0xF000:F000 */
    *(uint16_t *)(ram_base + 0x98) = 0x0000;
    *(uint16_t *)(ram_base + 0x9a) = 4096;   /* Video Buffer Capacity (4096 bytes) */
    *(uint16_t *)(ram_base + 0x9c) = 0x0000; /* Segment */
    *(uint16_t *)(ram_base + 0x9e) = 0x0000; /* Video Text Framebuffer Destination Offset */
    *(uint16_t *)(ram_base + 0xa0) = 0xb800; /* Video Text Framebuffer Destination Segment (0x000B8000) */
    *(uint16_t *)(ram_base + 0xa2) = 0xb800; /* Secondary Segment */
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
    *(uint16_t *)(ram_base + 0x41a) = 0x001e; /* Keyboard Buffer Head (0x001E) */
    *(uint16_t *)(ram_base + 0x41c) = 0x001e; /* Keyboard Buffer Tail (0x001E) */
    *(uint16_t *)(ram_base + 0x480) = 0x001e; /* Keyboard Buffer Start */
    *(uint16_t *)(ram_base + 0x482) = 0x003e; /* Keyboard Buffer End */
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

    /* Initialize Interrupt Vector Table (IVT) Overlay for authentic Toshiba firmware */
    if (machine->firmware && (strstr(machine->firmware, "512") || strstr(machine->firmware, "TOSHIBA"))) {
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
        ivt[0x10] = 0xf000f000; /* F000:F000 (INT 10h Video Service at Port 0x180) */
        ivt[0x11] = 0xf000f84d; /* F000:F84D (Equipment List Services) */
        ivt[0x12] = 0xf000f841; /* F000:F841 (Memory Size Services) */
        ivt[0x13] = 0xf000ec59; /* F000:EC59 (Fixed Disk & Floppy Services) */
        ivt[0x14] = 0xf000e739; /* F000:E739 (Serial Communications) */
        ivt[0x15] = 0xf000f859; /* F000:F859 (System & ACPI E820 Services) */
        ivt[0x16] = 0xf000e82e; /* F000:E82E (Keyboard Input Services) */
        ivt[0x17] = 0xf000efd2; /* F000:EFD2 (Parallel Printer Services) */
        ivt[0x18] = 0xf000e2c3; /* F000:E2C3 (ROM BASIC / Boot Failure) */
        ivt[0x19] = 0xf000e6f2; /* F000:E6F2 (Bootstrap OS Loader) */
        ivt[0x1a] = 0xf000fe6e; /* F000:FE6E (Real-Time Clock Services) */
        ivt[0x1c] = 0xf000ff53; /* F000:FF53 (User Timer Hook IRET) */
        ivt[0x1e] = 0xf000efc7; /* F000:EFC7 (Floppy Parameter Table) */

        /* Install default stubs in RAM F0000 */
        uint8_t *f0000_ptr = g_shadow_ram_f0000 ? (uint8_t *)memory_region_get_ram_ptr(g_shadow_ram_f0000) : (ram_base + 0xf0000);
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
    }

    /* Initialize BDA Video Variables (0x0040:0x0049..0x0089) for VGA Color Display */
    toshiba_system_ram = (uint8_t *)memory_region_get_ram_ptr(machine->ram);
    nv34m_set_system_ram(toshiba_system_ram);

    /* Authentically driven by Toshiba System BIOS GDT and PE transitions */
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

    /*
     * M306K5 LPC host mailbox (data 62h, command/status 66h).
     *
     * DBBSTS user flags are software-defined and reset to zero in the
     * silicon.  Toshiba BIOS requires bits 6:4 set during its F002 response
     * receive path, so model 70h as M200 EC-firmware/board initial state.
     */
    m200_ec_mailbox_new(OBJECT(machine), system_io, 0x70);


    /* Port 0x64 KBC Status (SYSF set) */
    MemoryRegion *kbc_status_io = g_new(MemoryRegion, 1);
    memory_region_init_io(kbc_status_io, OBJECT(machine), &toshiba_kbc_status_ops, NULL, "m200-kbc-status", 1);
    memory_region_add_subregion_overlap(system_io, 0x64, kbc_status_io, 50);

    /* Intel ICH4-M SMBus Controller (Base 0xD880) */
    ToshibaSMBusState *smbus = g_new0(ToshibaSMBusState, 1);
    smbus->lpc = ICH4M_LPC_DEVICE(lpc);
    smbus->bus = pcms->smbus;
    smbus->trace = getenv("M200_SMBUS_TRACE") != NULL;
    smbus->transaction_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                             m200_smbus_transaction_complete,
                                             smbus);
    MemoryRegion *smbus_io = g_new(MemoryRegion, 1);
    memory_region_init_io(smbus_io, OBJECT(machine), &toshiba_smbus_ops, smbus, "m200-smbus", 0x20);
    memory_region_add_subregion_overlap(system_io, 0xd880, smbus_io, 100);

    qemu_register_reset(pc_m200_reset_handler, x86ms);
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
    m->no_floppy = 0;
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
