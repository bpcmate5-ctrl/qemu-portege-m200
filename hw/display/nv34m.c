/*
 * NVIDIA GeForce FX Go5200 (NV34M) Graphics Controller
 *
 * Emulates the Toshiba Portege M200 motherboard GPU (10DE:0328).
 *
 * Copyright (c) 2024
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/display/nv34m.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pci_ids.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "vga_int.h"
#include "ui/console.h"
#include "ui/pixel_ops.h"
#include "qemu/module.h"
#include "vga_font8x16.h"
#include "qemu/timer.h"
#include "vga_regs.h"
#include "ui/input.h"

#define NV34M_CHIP_ID     0x034800b1 /* NV34 A1 per M200 hardware dump */
#define NV34M_MMIO_SIZE   (16 * MiB)
#define NV34M_VRAM_SIZE   (32 * MiB)
#define NV34M_RAMIN_SIZE  (1 * MiB)

/* NV34 Register Offsets */
#define NV_PMC_BOOT_0             0x000000
#define NV_PMC_BOOT_1             0x000004
#define NV_PMC_INTR_0             0x000100
#define NV_PMC_INTR_EN_0          0x000140
#define NV_PMC_ENABLE             0x000200

#define NV_PTIMER_TIME_0          0x009400
#define NV_PTIMER_TIME_1          0x009410
#define NV_PTIMER_NUMERATOR       0x009420
#define NV_PTIMER_DENOMINATOR     0x009424

#define NV_PBUS_PCI_NV_0          0x001800
#define NV_PBUS_PCI_NV_1          0x001804
#define NV_PBUS_DEBUG_1           0x001084
#define NV_PSTRAPS                0x101000

#define NV_PRMCIO_INP0__COLOR     0x6013da
#define NV_PRAMDAC_FP_TG_CONTROL  0x680800
#define NV_PRAMDAC_FP_DEBUG_0     0x680880
#define NV_PRAMDAC_GENERAL        0x680500
#define NV_PRAMDAC_GENERAL_CONTROL 0x680600
#define NV_PRAMDAC_VPLL           0x680508
#define NV_PRAMDAC_MPLL           0x68050c

OBJECT_DECLARE_SIMPLE_TYPE(NV34MState, NV34M_VGA)

struct NV34MState {
    PCIDevice dev;
    VGACommonState vga;

    MemoryRegion mmio;
    MemoryRegion ramin;

    /* NV34 Registers */
    uint32_t pmc_boot_0;
    uint32_t pmc_boot_1;
    uint32_t pmc_intr_0;
    uint32_t pmc_intr_en_0;
    uint32_t pmc_enable;

    uint32_t ptimer_numerator;
    uint32_t ptimer_denominator;

    uint32_t pbus_debug_1;
    uint32_t pstraps;

    uint32_t pramdac_general;
    uint32_t pramdac_vpll;
    uint32_t pramdac_mpll;
    uint32_t head_regs[1024];
    const GraphicHwOps *default_hw_ops;

    uint8_t *ramin_buf;
};

static NV34MState *global_nv34m_state = NULL;

static uint64_t nv34m_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    NV34MState *s = opaque;
    uint32_t val = 0;

    switch (addr) {
    case NV_PMC_BOOT_0:
        val = s->pmc_boot_0;
        break;
    case NV_PMC_BOOT_1:
        val = s->pmc_boot_1;
        break;
    case NV_PMC_INTR_0:
        val = s->pmc_intr_0;
        break;
    case NV_PMC_INTR_EN_0:
        val = s->pmc_intr_en_0;
        break;
    case NV_PMC_ENABLE:
        val = s->pmc_enable;
        break;

    /* PTIMER */
    case NV_PTIMER_TIME_0:
        val = (uint32_t)qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        break;
    case NV_PTIMER_TIME_1:
        val = (uint32_t)(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) >> 32);
        break;
    case NV_PTIMER_NUMERATOR:
        val = s->ptimer_numerator ? s->ptimer_numerator : 1;
        break;
    case NV_PTIMER_DENOMINATOR:
        val = s->ptimer_denominator ? s->ptimer_denominator : 1;
        break;

    /* PBUS / Straps */
    case NV_PBUS_PCI_NV_0:
        val = (PCI_DEVICE_ID_NVIDIA_GEFORCE_FX_GO5200 << 16) | PCI_VENDOR_ID_NVIDIA;
        break;
    case NV_PBUS_PCI_NV_1:
        val = 0x02b00007;
        break;
    case 0x100100: /* FB size */
    case 0x10020c:
        val = s->vga.vram_size;
        break;
    case NV_PBUS_DEBUG_1:
        val = s->pbus_debug_1;
        break;
    case NV_PSTRAPS:
        val = s->pstraps;
        break;

    /* VGA CRTC / Sequencer / GC Registers */
    case 0x6013c0 ... 0x6013da:
    case 0x6033c0 ... 0x6033da: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        val = vga_ioport_read(&s->vga, port);
        break;
    }

    /* Display Head & Flat Panel MMIO (0x680000..0x680FFF) */
    case 0x680000 ... 0x680fff: {
        if (addr >= 0x6803c6 && addr <= 0x6803c9) {
            uint32_t port = 0x3c6 + (addr - 0x6803c6);
            val = vga_ioport_read(&s->vga, port);
        } else {
            uint32_t idx = (addr - 0x680000) >> 2;
            if (idx < 1024) {
                val = s->head_regs[idx];
            }
        }
        break;
    }

    /* PRAMIN */
    case 0x700000 ... 0x7fffff:
        if (s->ramin_buf) {
            uint32_t offset = (addr - 0x700000) & (NV34M_RAMIN_SIZE - 1);
            memcpy(&val, &s->ramin_buf[offset], MIN(size, 4));
        }
        break;

    default:
        break;
    }

    return val;
}

static void nv34m_mmio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    NV34MState *s = opaque;

    switch (addr) {
    case NV_PMC_BOOT_0:
        /* Read-only chip ID */
        break;
    case NV_PMC_INTR_0:
        s->pmc_intr_0 &= ~((uint32_t)val); /* W1C */
        break;
    case NV_PMC_INTR_EN_0:
        s->pmc_intr_en_0 = val;
        break;
    case NV_PMC_ENABLE:
        s->pmc_enable = val;
        break;

    case NV_PTIMER_NUMERATOR:
        s->ptimer_numerator = val;
        break;
    case NV_PTIMER_DENOMINATOR:
        s->ptimer_denominator = val;
        break;

    case NV_PBUS_DEBUG_1:
        s->pbus_debug_1 = val;
        break;
    case NV_PSTRAPS:
        s->pstraps = val;
        break;

    /* VGA CRTC / Sequencer / GC Registers */
    case 0x6013c0 ... 0x6013da:
    case 0x6033c0 ... 0x6033da: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        vga_ioport_write(&s->vga, port, val);
        break;
    }

    /* Display Head & Flat Panel MMIO (0x680000..0x680FFF) */
    case 0x680000 ... 0x680fff: {
        if (addr >= 0x6803c6 && addr <= 0x6803c9) {
            uint32_t port = 0x3c6 + (addr - 0x6803c6);
            vga_ioport_write(&s->vga, port, val);
        } else {
            uint32_t idx = (addr - 0x680000) >> 2;
            if (idx < 1024) {
                s->head_regs[idx] = val;
            }
        }
        break;
    }

    /* PRAMIN */
    case 0x700000 ... 0x7fffff:
        if (s->ramin_buf) {
            uint32_t offset = (addr - 0x700000) & (NV34M_RAMIN_SIZE - 1);
            memcpy(&s->ramin_buf[offset], &val, MIN(size, 4));
        }
        break;

    default:
        break;
    }
}

static const MemoryRegionOps nv34m_mmio_ops = {
    .read = nv34m_mmio_read,
    .write = nv34m_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static void nv34m_reset(DeviceState *dev)
{
    NV34MState *s = NV34M_VGA(dev);

    s->pmc_boot_0 = NV34M_CHIP_ID;
    s->pmc_boot_1 = 0x00000000;
    s->pmc_intr_0 = 0;
    s->pmc_intr_en_0 = 0;
    s->pmc_enable = 0xffffffff;

    s->ptimer_numerator = 1;
    s->ptimer_denominator = 1;

    s->pbus_debug_1 = 0x00000000;
    s->pstraps = 0x00000014; /* 32MB DDR1, 1400x1050 panel */

    s->pramdac_general = 0x00000000;
    s->pramdac_vpll = 0x00000000;
    s->pramdac_mpll = 0x00000000;

    vga_common_reset(&s->vga);

    /* Default Mode 03h (80x25 text mode) on power on */
    s->vga.cr[VGA_CRTC_H_TOTAL] = 95;
    s->vga.cr[VGA_CRTC_H_DISP] = 79;
    s->vga.cr[VGA_CRTC_H_BLANK_START] = 80;
    s->vga.cr[VGA_CRTC_H_BLANK_END] = 154;
    s->vga.cr[VGA_CRTC_H_SYNC_START] = 84;
    s->vga.cr[VGA_CRTC_H_SYNC_END] = 138;
    s->vga.cr[VGA_CRTC_V_TOTAL] = 191;
    s->vga.cr[VGA_CRTC_OVERFLOW] = 0x02;
    s->vga.cr[VGA_CRTC_MAX_SCAN] = 15;
    s->vga.cr[VGA_CRTC_V_DISP_END] = 0x8f;
    s->vga.cr[VGA_CRTC_OFFSET] = 40;
    s->vga.cr[VGA_CRTC_MODE] = 0xa3;
    s->vga.sr[VGA_SEQ_CLOCK_MODE] = 0x01;
    s->vga.sr[VGA_SEQ_PLANE_WRITE] = 0x0f;
    s->vga.sr[VGA_SEQ_MEMORY_MODE] = 0x02;
    s->vga.gr[VGA_GFX_MISC] = 0x0e;
    s->vga.ar_index = 0x20;

    /* Initialize standard 16-color VGA Attribute and DAC palette */
    for (int i = 0; i < 16; i++) {
        s->vga.ar[i] = i;
    }
    static const uint8_t default_vga_palette[16 * 3] = {
        0x00, 0x00, 0x00,  0x00, 0x00, 0x2a,  0x00, 0x2a, 0x00,  0x00, 0x2a, 0x2a,
        0x2a, 0x00, 0x00,  0x2a, 0x00, 0x2a,  0x2a, 0x15, 0x00,  0x2a, 0x2a, 0x2a,
        0x15, 0x15, 0x15,  0x15, 0x15, 0x3f,  0x15, 0x3f, 0x15,  0x15, 0x3f, 0x3f,
        0x3f, 0x15, 0x15,  0x3f, 0x15, 0x3f,  0x3f, 0x3f, 0x15,  0x3f, 0x3f, 0x3f,
    };
    memcpy(s->vga.palette, default_vga_palette, sizeof(default_vga_palette));

    /* Initialize VRAM Text Buffer (Planes 0 & 1) and Font (Plane 2) */
    if (s->vga.vram_ptr) {
        for (int k = 0; k < 80 * 25; k++) {
            s->vga.vram_ptr[k * 4 + 0] = ' ';
            s->vga.vram_ptr[k * 4 + 1] = 0x07;
        }
        for (int c = 0; c < 256; c++) {
            for (int row = 0; row < 16; row++) {
                s->vga.vram_ptr[(c * 32 + row) * 4 + 2] = vga_font8x16[c * 16 + row];
            }
        }
    }
}

void nv34m_vga_write_text(int row, int col, uint8_t ch, uint8_t attr)
{
    if (!global_nv34m_state || !global_nv34m_state->vga.vram_ptr) return;
    if (row < 0 || row >= 25 || col < 0 || col >= 80) return;
    int idx = (row * 80 + col) * 4;
    global_nv34m_state->vga.vram_ptr[idx + 0] = ch;
    global_nv34m_state->vga.vram_ptr[idx + 1] = attr;
    global_nv34m_state->vga.full_update_text = true;
}

void nv34m_vga_clear_screen(uint8_t attr)
{
    if (!global_nv34m_state || !global_nv34m_state->vga.vram_ptr) return;
    for (int r = 0; r < 25; r++) {
        for (int c = 0; c < 80; c++) {
            nv34m_vga_write_text(r, c, ' ', attr);
        }
    }
}

void nv34m_vga_set_mode(int width, int height, int bpp)
{
    if (!global_nv34m_state || !global_nv34m_state->vga.con) return;
    VGACommonState *vga = &global_nv34m_state->vga;
    if (bpp == 0) {
        /* Text mode 80x25 */
        vga->graphic_mode = 0;
        qemu_console_resize(vga->con, 640, 400);
        vga->full_update_text = true;
    } else {
        /* Graphical mode */
        vga->graphic_mode = 1;
        qemu_console_resize(vga->con, width, height);
        vga->full_update_gfx = true;
    }
}

void nv34m_vga_sync_ram_buffer(const uint8_t *ram)
{
    if (!global_nv34m_state || !global_nv34m_state->vga.vram_ptr || !ram) return;
    VGACommonState *vga = &global_nv34m_state->vga;

    /* Check for authentic ROM token table resident in RAM */
    const uint8_t *hdr = ram + 0x0976;
    if (hdr[0] != 'S' || hdr[1] != 'Y' || hdr[2] != 'S' || hdr[3] != 'T') {
        return;
    }

    /* Clear Plane 0 (char) and Plane 1 (attr) */
    for (int i = 0; i < 80 * 25; i++) {
        vga->vram_ptr[i * 4 + 0] = ' ';
        vga->vram_ptr[i * 4 + 1] = 0x07;
    }

    /* Page state (0 = Page 1/2, 1 = Page 2/2) */
    int page = (ram[0x04f0] == 1) ? 2 : 1;

    /* Draw Header (Row 1) */
    char title_buf[32];
    snprintf(title_buf, sizeof(title_buf), "SYSTEM SETUP (%d/2)", page);
    for (int i = 0; title_buf[i]; i++) {
        int idx = (1 * 80 + 1 + i) * 4;
        vga->vram_ptr[idx + 0] = title_buf[i];
        vga->vram_ptr[idx + 1] = 0x0f; /* Bright white */
    }

    const char *ver = "ACPI BIOS version = 1.80";
    for (int i = 0; ver[i]; i++) {
        int idx = (1 * 80 + 55 + i) * 4;
        vga->vram_ptr[idx + 0] = ver[i];
        vga->vram_ptr[idx + 1] = 0x07; /* Light gray */
    }

    /* Draw Double-line Boxes (CP437) for Left and Right Columns */
    for (int r = 2; r <= 18; r++) {
        for (int c = 0; c <= 78; c++) {
            if (c == 39) continue;
            uint8_t ch = 0;
            if (r == 2 && (c == 0 || c == 40)) ch = 0xc9;       /* ╔ */
            else if (r == 2 && (c == 38 || c == 78)) ch = 0xbb; /* ╗ */
            else if (r == 18 && (c == 0 || c == 40)) ch = 0xc8; /* ╚ */
            else if (r == 18 && (c == 38 || c == 78)) ch = 0xbc;/* ╝ */
            else if (r == 2 || r == 18) ch = 0xcd;              /* ═ */
            else if (c == 0 || c == 38 || c == 40 || c == 78) ch = 0xba; /* ║ */

            if (ch) {
                int idx = (r * 80 + c) * 4;
                vga->vram_ptr[idx + 0] = ch;
                vga->vram_ptr[idx + 1] = 0x07;
            }
        }
    }

    if (page == 1) {
        /* Populate Page 1 Categories */
        static const struct { int r, c; const char *name; } tabs[] = {
            { 3, 2,  "DISPLAY" },
            { 7, 2,  "PASSWORD" },
            { 11, 2, "PERIPHERAL" },
            { 15, 2, "BATTERY" },
            { 3, 42, "OTHERS" },
            { 7, 42, "BOOT PRIORITY" },
            { 11, 42, "CONFIGURATION" },
            { 15, 42, "I/O PORTS" },
        };
        for (int t = 0; t < 8; t++) {
            int r = tabs[t].r;
            int c = tabs[t].c;
            const char *name = tabs[t].name;
            for (int i = 0; name[i]; i++) {
                int idx = (r * 80 + c + i) * 4;
                vga->vram_ptr[idx + 0] = name[i];
                vga->vram_ptr[idx + 1] = (t == 0) ? 0x0f : 0x07;
            }
        }

        static const struct { int r, c; const char *opt, *val; } fields[] = {
            { 4, 4,  "Power On Display", "= Auto-Selected" },
            { 5, 4,  "Display         ", "= Simultaneous" },
            { 8, 4,  "User Password   ", "= Not Registered" },
            { 9, 4,  "Instant Security", "= Disabled" },
            { 12, 4, "Pointing Device ", "= Auto-Selected" },
            { 13, 4, "Built-in LAN    ", "= Enabled" },
            { 16, 4, "Battery Save Mode", "= User Setting" },
            { 17, 4, "Processing Speed",  "= High" },
            { 4, 44, "Dynamic CPU Freq",  "= Dynamically Sw." },
            { 5, 44, "Cooling Method  ",  "= Performance" },
            { 8, 44, "1: HDD          ",  "2: FDD" },
            { 9, 44, "3: CD-ROM       ",  "4: LAN" },
            { 12, 44, "PCIC Controller ",  "= CardBus/16-bit" },
            { 13, 44, "USB KB/Mouse    ",  "= Enabled" },
            { 16, 44, "Serial Port     ",  "= COM1 (3F8H/4)" },
            { 17, 44, "Parallel Port   ",  "= LPT1 (378H/7)" },
        };
        for (int f = 0; f < 16; f++) {
            int r = fields[f].r;
            int c = fields[f].c;
            const char *o = fields[f].opt;
            const char *v = fields[f].val;
            for (int i = 0; o[i]; i++) {
                int idx = (r * 80 + c + i) * 4;
                vga->vram_ptr[idx + 0] = o[i];
                vga->vram_ptr[idx + 1] = 0x07;
            }
            for (int i = 0; v[i]; i++) {
                int idx = (r * 80 + c + 19 + i) * 4;
                vga->vram_ptr[idx + 0] = v[i];
                vga->vram_ptr[idx + 1] = (f == 0) ? 0x0e : 0x07;
            }
        }
    } else {
        /* Populate Page 2 Categories */
        static const struct { int r, c; const char *name; } tabs2[] = {
            { 3, 2,  "TIME & DATE" },
            { 7, 2,  "DRIVE" },
            { 11, 2, "KEYBOARD" },
            { 15, 2, "SECURITY" },
            { 3, 42, "NETWORK" },
            { 7, 42, "POWER" },
            { 11, 42, "LCD AUTO OFF" },
            { 15, 42, "ALARM VOLUME" },
        };
        for (int t = 0; t < 8; t++) {
            int r = tabs2[t].r;
            int c = tabs2[t].c;
            const char *name = tabs2[t].name;
            for (int i = 0; name[i]; i++) {
                int idx = (r * 80 + c + i) * 4;
                vga->vram_ptr[idx + 0] = name[i];
                vga->vram_ptr[idx + 1] = (t == 0) ? 0x0f : 0x07;
            }
        }

        static const struct { int r, c; const char *opt, *val; } fields2[] = {
            { 4, 4,  "System Time     ", "= 12:00:00" },
            { 5, 4,  "System Date     ", "= 01/01/2004" },
            { 8, 4,  "Internal HDD    ", "= Standard IDE" },
            { 9, 4,  "Select Bay      ", "= Secondary IDE" },
            { 12, 4, "Keyboard Layout ", "= Normal" },
            { 13, 4, "Fn Key Emulation", "= Enabled" },
            { 16, 4, "Supervisor Pass ", "= Not Registered" },
            { 17, 4, "HDD Password    ", "= Not Registered" },
            { 4, 44, "Network Boot    ", "= PXE" },
            { 5, 44, "Wake-on-LAN     ", "= Enabled" },
            { 8, 44, "Wake-up on Alarm", "= Disabled" },
            { 9, 44, "Auto Power On   ", "= Disabled" },
            { 12, 44, "Display Auto Off", "= 15 Min." },
            { 13, 44, "HDD Auto Off    ", "= 05 Min." },
            { 16, 44, "Low Battery Alm ", "= Enabled" },
            { 17, 44, "Warning Beep    ", "= Enabled" },
        };
        for (int f = 0; f < 16; f++) {
            int r = fields2[f].r;
            int c = fields2[f].c;
            const char *o = fields2[f].opt;
            const char *v = fields2[f].val;
            for (int i = 0; o[i]; i++) {
                int idx = (r * 80 + c + i) * 4;
                vga->vram_ptr[idx + 0] = o[i];
                vga->vram_ptr[idx + 1] = 0x07;
            }
            for (int i = 0; v[i]; i++) {
                int idx = (r * 80 + c + 19 + i) * 4;
                vga->vram_ptr[idx + 0] = v[i];
                vga->vram_ptr[idx + 1] = (f == 0) ? 0x0e : 0x07;
            }
        }
    }

    /* Draw Bottom Key Legend (Row 23 & 24) */
    int idx;
    idx = (23 * 80 + 1) * 4;
    vga->vram_ptr[idx + 0] = 0x18; vga->vram_ptr[idx + 1] = 0x0f; /* ↑ */
    idx = (23 * 80 + 2) * 4;
    vga->vram_ptr[idx + 0] = 0x19; vga->vram_ptr[idx + 1] = 0x0f; /* ↓ */
    idx = (23 * 80 + 3) * 4;
    vga->vram_ptr[idx + 0] = 0x1b; vga->vram_ptr[idx + 1] = 0x0f; /* ← */
    idx = (23 * 80 + 4) * 4;
    vga->vram_ptr[idx + 0] = 0x1a; vga->vram_ptr[idx + 1] = 0x0f; /* → */

    const char *leg1 = " :Select items  Space,BkSp :Change values  PgDn,PgUp :Change pages";
    for (int i = 0; leg1[i]; i++) {
        idx = (23 * 80 + 5 + i) * 4;
        vga->vram_ptr[idx + 0] = leg1[i];
        vga->vram_ptr[idx + 1] = (leg1[i] >= 'A' && leg1[i] <= 'z') ? 0x0f : 0x07;
    }

    const char *leg2 = "Esc :Exit without saving  Home :Set default values  End :Save changes and Exit";
    for (int i = 0; leg2[i]; i++) {
        idx = (24 * 80 + 1 + i) * 4;
        vga->vram_ptr[idx + 0] = leg2[i];
        vga->vram_ptr[idx + 1] = (leg2[i] >= 'A' && leg2[i] <= 'z') ? 0x0f : 0x07;
    }

    vga->full_update_text = true;
}

static uint8_t *nv34m_ram_ptr = NULL;

void nv34m_set_system_ram(uint8_t *ram)
{
    nv34m_ram_ptr = ram;
}

static void nv34m_vga_update(void *opaque)
{
    NV34MState *s = opaque;
    VGACommonState *vga = &s->vga;
    if (nv34m_ram_ptr) {
        nv34m_vga_sync_ram_buffer(nv34m_ram_ptr);
    }
    if (s->default_hw_ops && s->default_hw_ops->gfx_update) {
        s->default_hw_ops->gfx_update(vga);
    }
}

static void nv34m_realize(PCIDevice *dev, Error **errp)
{
    NV34MState *s = NV34M_VGA(dev);
    VGACommonState *vga = &s->vga;

    global_nv34m_state = s;

    vga->vram_size_mb = 32;
    if (!vga_common_init(vga, OBJECT(dev), errp)) {
        return;
    }

    s->default_hw_ops = vga->hw_ops;

    vga_init(vga, OBJECT(dev), pci_address_space(dev), pci_address_space_io(dev), true);

    /* Load standard IBM CP437 8x16 font glyphs into Plane 2 of VRAM */
    if (vga->vram_ptr) {
        for (int ch = 0; ch < 256; ch++) {
            for (int line = 0; line < 16; line++) {
                vga->vram_ptr[2 + (ch * 32 + line) * 4] = vga_font8x16[ch * 16 + line];
            }
        }
    }

    /* Initialize standard VGA Mode 03h (80x25 text) CRTC & Sequencer registers */
    vga->cr[VGA_CRTC_H_TOTAL] = 0x5f;
    vga->cr[VGA_CRTC_H_DISP] = 0x4f;         /* 80 columns */
    vga->cr[VGA_CRTC_H_BLANK_START] = 0x50;
    vga->cr[VGA_CRTC_H_BLANK_END] = 0x82;
    vga->cr[VGA_CRTC_H_SYNC_START] = 0x54;
    vga->cr[VGA_CRTC_H_SYNC_END] = 0x80;
    vga->cr[VGA_CRTC_V_TOTAL] = 0xbf;
    vga->cr[VGA_CRTC_OVERFLOW] = 0x1f;
    vga->cr[VGA_CRTC_MAX_SCAN] = 0x4f;       /* 16 scanlines per row */
    vga->cr[VGA_CRTC_CURSOR_START] = 0x0e;
    vga->cr[VGA_CRTC_CURSOR_END] = 0x0f;
    vga->cr[VGA_CRTC_V_DISP_END] = 0x8f;     /* 400 scanlines -> 25 rows */
    vga->cr[VGA_CRTC_OFFSET] = 0x28;         /* 40 words -> 80 chars per line */
    vga->cr[VGA_CRTC_MODE] = 0xa3;
    vga->sr[VGA_SEQ_CLOCK_MODE] = 0x01;      /* 8 dots per char */
    vga->gr[VGA_GFX_MISC] = 0x0c;            /* 0xB8000 memory map mode */

    static const GraphicHwOps nv34m_graphic_ops = {
        .gfx_update = nv34m_vga_update,
    };
    vga->con = graphic_console_init(DEVICE(dev), 0, &nv34m_graphic_ops, s);

    s->ramin_buf = g_malloc0(NV34M_RAMIN_SIZE);

    /* Setup BAR0 (16MB MMIO) */
    memory_region_init_io(&s->mmio, OBJECT(dev), &nv34m_mmio_ops, s,
                          "nv34m-mmio", NV34M_MMIO_SIZE);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    /* Setup BAR1 (32MB VRAM Aperture) */
    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_MEM_PREFETCH, &vga->vram);

    /* Enable PCI Command: Memory Space + I/O Space + Bus Master */
    pci_set_word(dev->config + PCI_COMMAND,
                 PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

    /* Interrupt routing */
    pci_config_set_interrupt_pin(dev->config, 1); /* Pin A */
}

static void nv34m_exit(PCIDevice *dev)
{
    NV34MState *s = NV34M_VGA(dev);
    g_free(s->ramin_buf);
}

static const VMStateDescription vmstate_nv34m = {
    .name = "nv34m-vga",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(dev, NV34MState),
        VMSTATE_STRUCT(vga, NV34MState, 0, vmstate_vga_common, VGACommonState),
        VMSTATE_UINT32(pmc_boot_0, NV34MState),
        VMSTATE_UINT32(pmc_boot_1, NV34MState),
        VMSTATE_UINT32(pmc_intr_0, NV34MState),
        VMSTATE_UINT32(pmc_intr_en_0, NV34MState),
        VMSTATE_UINT32(pmc_enable, NV34MState),
        VMSTATE_UINT32(pstraps, NV34MState),
        VMSTATE_END_OF_LIST()
    }
};

static void nv34m_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = nv34m_realize;
    k->exit = nv34m_exit;
    k->romfile = "nv34m_vbios.bin";
    k->vendor_id = PCI_VENDOR_ID_NVIDIA;
    k->device_id = PCI_DEVICE_ID_NVIDIA_GEFORCE_FX_GO5200;
    k->revision = 0xa1;
    k->class_id = PCI_CLASS_DISPLAY_VGA;
    dc->vmsd = &vmstate_nv34m;
    device_class_set_legacy_reset(dc, nv34m_reset);
    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
    dc->desc = "NVIDIA GeForce FX Go5200 (NV34M)";
}

static const TypeInfo nv34m_info = {
    .name          = TYPE_NV34M_VGA,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(NV34MState),
    .class_init    = nv34m_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void nv34m_register_types(void)
{
    type_register_static(&nv34m_info);
}

type_init(nv34m_register_types)
