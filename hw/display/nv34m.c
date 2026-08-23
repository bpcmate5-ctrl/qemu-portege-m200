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
#include "ui/input.h"
#include "qemu/module.h"
#include "vga_font8x16.h"
#include "qemu/timer.h"
#include "vga_regs.h"

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

#define NV_PBUS_PCI_NV_0          0x100000
#define NV_PBUS_PCI_NV_1          0x100004
#define NV_PBUS_DEBUG_1           0x100200
#define NV_PSTRAPS                0x101000

#define NV_PRAMDAC_GENERAL        0x680500
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

    uint8_t *ramin_buf;

    int bios_screen_mode;
    QemuInputHandlerState *kbd;
};

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

    /* PRAMDAC */
    case NV_PRAMDAC_GENERAL:
        val = s->pramdac_general;
        break;
    case NV_PRAMDAC_VPLL:
        val = s->pramdac_vpll;
        break;
    case NV_PRAMDAC_MPLL:
        val = s->pramdac_mpll;
        break;

    /* VGA CRTC / Sequencer / GC Registers */
    case 0x6013c0 ... 0x6013da:
    case 0x6033c0 ... 0x6033da: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        val = vga_ioport_read(&s->vga, port);
        break;
    }

    /* VGA DAC Registers */
    case 0x6803c6 ... 0x6803c9: {
        uint32_t port = 0x3c6 + (addr - 0x6803c6);
        val = vga_ioport_read(&s->vga, port);
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

    case NV_PRAMDAC_GENERAL:
        s->pramdac_general = val;
        break;
    case NV_PRAMDAC_VPLL:
        s->pramdac_vpll = val;
        break;
    case NV_PRAMDAC_MPLL:
        s->pramdac_mpll = val;
        break;

    /* VGA CRTC / Sequencer / GC Registers */
    case 0x6013c0 ... 0x6013da:
    case 0x6033c0 ... 0x6033da: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        vga_ioport_write(&s->vga, port, val);
        break;
    }

    /* VGA DAC Registers */
    case 0x6803c6 ... 0x6803c9: {
        uint32_t port = 0x3c6 + (addr - 0x6803c6);
        vga_ioport_write(&s->vga, port, val);
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

static void nv34m_render_screen(NV34MState *s, int mode);

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

    /* Load standard 8x16 font into VRAM Plane 2 */
    if (s->vga.vram_ptr) {
        for (int c = 0; c < 256; c++) {
            for (int row = 0; row < 16; row++) {
                s->vga.vram_ptr[(c * 32 + row) * 4 + 2] = vga_font8x16[c * 16 + row];
            }
        }

        for (int i = 0; i < 80 * 25; i++) {
            s->vga.vram_ptr[i * 4 + 0] = ' ';
            s->vga.vram_ptr[i * 4 + 1] = 0x07;
        }
    }
}

static const char *bios_screens[4][25] = {
    /* Screen 0: NoteBIOS POST Screen */
    {
        "================================================================================",
        "                           TOSHIBA Portege M200                                 ",
        "                     Tablet PC NoteBIOS (v1.00 EC45)                            ",
        "================================================================================",
        " CPU Type       : Intel(R) Pentium(R) M processor 740 @ 1.73GHz                 ",
        " CPU Speed      : 1.73 GHz (FSB 533 MHz)                                        ",
        " System Memory  : 512 MB DDR-333 SDRAM (Dual Channel Active)                    ",
        " Host Chipset   : Intel 82855PM Memory Controller Hub (MCH)                     ",
        " Southbridge    : Intel 82801DBM (ICH4-M) I/O Controller Hub                    ",
        " Video Device   : NVIDIA GeForce FX Go5200 (NV34M 32MB DDR VRAM)               ",
        " Internal Panel : 12.1\" Poly-silicon SXGA+ (1400 x 1050) TFT LCD               ",
        " Audio Device   : Intel 82801DBM AC'97 Audio Controller                         ",
        " Digitizer      : Wacom Serial Pen Tablet Digitizer (COM4 / IRQ 4)              ",
        " IDE Primary    : 60.0 GB ATA/100 Hard Disk Drive                               ",
        " IDE Secondary  : TOSHIBA DVD-ROM / CD-RW Slimline Drive                        ",
        "================================================================================",
        " NoteBIOS POST Status: PASSED (All hardware subsystems OK)                      ",
        "                                                                                ",
        " Press <F2> for System Setup, <F12> for Boot Menu, <Enter> to Boot...           ",
        "================================================================================",
        "                                                                                ",
        "                                                                                ",
        "                                                                                ",
        "                                                                                ",
        "================================================================================"
    },
    /* Screen 1: SYSTEM SETUP (1/2) */
    {
        "================================================================================",
        "             TOSHIBA Portege M200 - SYSTEM SETUP (1/2) - NoteBIOS               ",
        "================================================================================",
        "  MEMORY            : Base 640 KB, Extended 523,264 KB, Total 512 MB            ",
        "                                                                                ",
        "  DISPLAY           : Internal LCD Only  [1400x1050 Poly-silicon TFT]           ",
        "  DISPLAY MODE      : Stretch Mode [Enabled]                                    ",
        "                                                                                ",
        "  PASSWORD          : User Password       [Not Registered]                      ",
        "  HDD PASSWORD      : Hard Disk Security  [Not Registered]                      ",
        "                                                                                ",
        "  PERIPHERAL        : Serial Port 1       (COM1 / 0x3F8 / IRQ4)                 ",
        "  TABLET DIGITIZER  : Wacom Digitizer Pen (COM4 / 0x338 / IRQ4)                 ",
        "                                                                                ",
        "  BATTERY SAVER     : High Performance    [S0 Full Power State]                 ",
        "  POWER MANAGEMENT  : ACPI 2.0 / APM 1.2  [Enabled]                             ",
        "                                                                                ",
        "  BOOT PRIORITY     : 1. HDD  2. CD-ROM  3. LAN (PXE)  4. USB FDD               ",
        "                                                                                ",
        "================================================================================",
        " [F2 / PgDn]: Page 2   |   [Space]: Change Value   |   [Esc / Enter]: Exit/Save ",
        "================================================================================",
        "                                                                                ",
        "                                                                                ",
        "================================================================================"
    },
    /* Screen 2: SYSTEM SETUP (2/2) */
    {
        "================================================================================",
        "             TOSHIBA Portege M200 - SYSTEM SETUP (2/2) - NoteBIOS               ",
        "================================================================================",
        "  I/O PORTS         : Parallel Port       (LPT1 / 0x378 / ECP+EPP)              ",
        "                                                                                ",
        "  PCI BUS           : PCI LAN Intel PRO/100 VE Network     [Enabled]            ",
        "  SOUND SYSTEM      : Intel 82801DBM AC'97 Audio Codec     [Enabled]            ",
        "  USB CONTROLLER    : Intel ICH4-M USB 2.0 (EHCI + 3xUHCI) [Enabled]            ",
        "  LEGACY USB        : USB Keyboard / Mouse Emulation       [Enabled]            ",
        "                                                                                ",
        "  PC CARD (CARDBUS) : TI PCI-1510 CardBus Controller       [Enabled]            ",
        "  SD CONTROLLER     : Toshiba SD Card Host Controller      [Enabled]            ",
        "                                                                                ",
        "  SYSTEM DATE       : 08 / 23 / 2026                                            ",
        "  SYSTEM TIME       : 06 : 28 : 00                                              ",
        "                                                                                ",
        "  VIRUS CHECK       : Built-in Boot Sector Protect         [Disabled]           ",
        "                                                                                ",
        "================================================================================",
        " [F2 / PgUp]: Page 1   |   [Space]: Change Value   |   [Esc / Enter]: Exit/Save ",
        "================================================================================",
        "                                                                                ",
        "                                                                                ",
        "================================================================================"
    },
    /* Screen 3: BOOT DEVICE MENU */
    {
        "================================================================================",
        "                     TOSHIBA Portege M200 - BOOT DEVICE MENU                    ",
        "================================================================================",
        "                                                                                ",
        "   Please select boot device:                                                   ",
        "                                                                                ",
        "      [ 1 ]  Primary Hard Disk Drive (60.0 GB ATA/100 HDD)                      ",
        "      [ 2 ]  Secondary CD-ROM / DVD Slimline Drive                              ",
        "      [ 3 ]  Intel(R) Boot Agent FE (PXE Network Boot)                          ",
        "      [ 4 ]  USB Flash Memory / Removable Drive                                 ",
        "                                                                                ",
        "                                                                                ",
        "                                                                                ",
        "                                                                                ",
        "                                                                                ",
        "                                                                                ",
        "                                                                                ",
        "                                                                                ",
        "                                                                                ",
        "================================================================================",
        " Use [1-4] or [Up/Down] arrows to select, [Enter] to Boot, [Esc] to Return      ",
        "================================================================================",
        "                                                                                ",
        "                                                                                ",
        "================================================================================"
    }
};

static void nv34m_render_screen(NV34MState *s, int mode)
{
    if (!s->vga.vram_ptr || mode < 0 || mode > 3) {
        return;
    }
    s->bios_screen_mode = mode;

    for (int r = 0; r < 25; r++) {
        const char *row_str = bios_screens[mode][r];
        if (!row_str) {
            continue;
        }
        for (int c = 0; c < 80 && row_str[c]; c++) {
            uint8_t ch = (uint8_t)row_str[c];
            uint8_t attr = 0x07;
            if (r < 4 || r >= 19) {
                attr = 0x1f; /* White on Blue headers/footers */
            } else if (mode == 0 && r == 16) {
                attr = 0x0a; /* Bright Green for PASSED */
            } else if (mode == 3 && r >= 6 && r <= 9) {
                attr = 0x0f; /* Bright White for menu choices */
            }
            s->vga.vram_ptr[(r * 80 + c) * 4 + 0] = ch;
            s->vga.vram_ptr[(r * 80 + c) * 4 + 1] = attr;
        }
    }
    s->vga.full_update_text = 1;
    graphic_hw_invalidate(s->vga.con);
}

static void nv34m_kbd_event(DeviceState *dev, QemuConsole *src, InputEvent *evt)
{
    NV34MState *s = NV34M_VGA(dev);
    if (evt->type != INPUT_EVENT_KIND_KEY || !evt->u.key.data->down) {
        return;
    }
    int qcode = qemu_input_key_value_to_qcode(evt->u.key.data->key);
    switch (qcode) {
    case Q_KEY_CODE_F2:
        if (s->bios_screen_mode == 1) {
            s->bios_screen_mode = 2; /* Switch to Page 2 */
        } else if (s->bios_screen_mode == 2) {
            s->bios_screen_mode = 1; /* Switch to Page 1 */
        } else {
            s->bios_screen_mode = 1; /* Open SYSTEM SETUP (1/2) */
        }
        nv34m_render_screen(s, s->bios_screen_mode);
        break;

    case Q_KEY_CODE_PGDN:
        if (s->bios_screen_mode == 1) {
            s->bios_screen_mode = 2;
            nv34m_render_screen(s, s->bios_screen_mode);
        }
        break;

    case Q_KEY_CODE_PGUP:
        if (s->bios_screen_mode == 2) {
            s->bios_screen_mode = 1;
            nv34m_render_screen(s, s->bios_screen_mode);
        }
        break;

    case Q_KEY_CODE_F12:
        s->bios_screen_mode = 3; /* BOOT DEVICE MENU */
        nv34m_render_screen(s, s->bios_screen_mode);
        break;

    case Q_KEY_CODE_ESC:
        if (s->bios_screen_mode != 0) {
            s->bios_screen_mode = 0; /* Return to POST */
            nv34m_render_screen(s, s->bios_screen_mode);
        }
        break;

    default:
        break;
    }
}

static const QemuInputHandler nv34m_kbd_handler = {
    .name  = "nv34m-kbd",
    .mask  = INPUT_EVENT_MASK_KEY,
    .event = nv34m_kbd_event,
};

static void nv34m_realize(PCIDevice *dev, Error **errp)
{
    NV34MState *s = NV34M_VGA(dev);
    VGACommonState *vga = &s->vga;

    vga->vram_size_mb = 32;
    if (!vga_common_init(vga, OBJECT(dev), errp)) {
        return;
    }

    vga_init(vga, OBJECT(dev), pci_address_space(dev), pci_address_space_io(dev), true);

    vga->con = graphic_console_init(DEVICE(dev), 0, vga->hw_ops, vga);

    s->ramin_buf = g_malloc0(NV34M_RAMIN_SIZE);

    /* Setup BAR0 (16MB MMIO) */
    memory_region_init_io(&s->mmio, OBJECT(dev), &nv34m_mmio_ops, s,
                          "nv34m-mmio", NV34M_MMIO_SIZE);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    /* Setup BAR1 (32MB VRAM Aperture) */
    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_MEM_PREFETCH, &vga->vram);

    /* Subsystem IDs */
    pci_set_word(dev->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
    pci_set_word(dev->config + PCI_SUBSYSTEM_ID, 0x0010);

    /* Interrupt routing */
    pci_config_set_interrupt_pin(dev->config, 1); /* Pin A */

    /* Register interactive keyboard handler */
    s->kbd = qemu_input_handler_register(DEVICE(dev), &nv34m_kbd_handler);
    qemu_input_handler_activate(s->kbd);
}

static void nv34m_exit(PCIDevice *dev)
{
    NV34MState *s = NV34M_VGA(dev);
    if (s->kbd) {
        qemu_input_handler_unregister(s->kbd);
    }
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
