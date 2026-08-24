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

/* =========================================================================
 * NV34 Register Definitions (Categorized by Engine Block)
 * ========================================================================= */

/* PMC (Master Control) 0x000000..0x000FFF */
#define NV_PMC_BOOT_0             0x000000
#define NV_PMC_BOOT_1             0x000004
#define NV_PMC_INTR_0             0x000100
#define NV_PMC_INTR_EN_0          0x000140
#define NV_PMC_ENABLE             0x000200

/* PBUS (PCI / Bus Interface) 0x001000..0x001FFF */
#define NV_PBUS_DEBUG_0           0x001080
#define NV_PBUS_DEBUG_1           0x001084
#define NV_PBUS_PCI_NV_0          0x001800
#define NV_PBUS_PCI_NV_1          0x001804

/* PTIMER (Timer & Timebase) 0x009000..0x009FFF */
#define NV_PTIMER_TIME_0          0x009400
#define NV_PTIMER_TIME_1          0x009410
#define NV_PTIMER_NUMERATOR       0x009420
#define NV_PTIMER_DENOMINATOR     0x009424

/* PFB (Memory Controller & Framebuffer) 0x100000..0x100FFF */
#define NV_PFB_CFG0               0x100080
#define NV_PFB_CFG1               0x100084
#define NV_PFB_CSTATUS            0x100100
#define NV_PFB_TIMING0            0x100200
#define NV_PFB_TIMING1            0x100204
#define NV_PFB_TIMING2            0x100208
#define NV_PFB_RAM_AMOUNT         0x10020C
#define NV_PFB_REFRESH            0x100210
#define NV_PFB_RAM_CFG            0x100220
#define NV_PFB_WBC                0x100400

/* PSTRAPS (Hardware Straps) 0x101000 */
#define NV_PSTRAPS                0x101000

/* PCRTC0 (Primary CRTC Controller) 0x600000..0x600FFF */
#define NV_PCRTC0_INTR            0x600100
#define NV_PCRTC0_INTR_EN         0x600140

/* PRMCIO0 (Primary VGA CRTC/Attr IO Shadow) 0x601000..0x601FFF */
#define NV_PRMCIO0_BASE           0x601000
#define NV_PRMCIO0_INP0__COLOR    0x6013da

/* PCRTC1 (Secondary CRTC Controller) 0x602000..0x602FFF */
#define NV_PCRTC1_INTR            0x602100
#define NV_PCRTC1_INTR_EN         0x602140

/* PRMCIO1 (Secondary VGA CRTC/Attr IO Shadow) 0x603000..0x603FFF */
#define NV_PRMCIO1_BASE           0x603000
#define NV_PRMCIO1_INP0__COLOR    0x6033da

/* PRAMDAC0 (Primary Display Output Engine) 0x680000..0x680FFF */
#define NV_PRAMDAC0_CURSOR_POS    0x680300
#define NV_PRAMDAC0_NVPLL         0x680500
#define NV_PRAMDAC0_MPLL          0x680504
#define NV_PRAMDAC0_VPLL          0x680508
#define NV_PRAMDAC0_PLL_CTRL      0x68050c
#define NV_PRAMDAC0_PLL_SETUP     0x680510
#define NV_PRAMDAC0_VPLL2         0x680520
#define NV_PRAMDAC0_SEL_CLK       0x680524
#define NV_PRAMDAC0_NVPLL_S2      0x680570
#define NV_PRAMDAC0_MPLL_S2       0x680574
#define NV_PRAMDAC0_VPLL_S2       0x680578
#define NV_PRAMDAC0_VPLL2_S2      0x68057c
#define NV_PRAMDAC0_GENERAL_CTRL  0x680600
#define NV_PRAMDAC0_FP_VDISP_END  0x680800
#define NV_PRAMDAC0_FP_VTOTAL     0x680804
#define NV_PRAMDAC0_FP_VCRTC      0x680808
#define NV_PRAMDAC0_FP_VSYNC_S    0x68080c
#define NV_PRAMDAC0_FP_VSYNC_E    0x680810
#define NV_PRAMDAC0_FP_HDISP_END  0x680820
#define NV_PRAMDAC0_FP_HTOTAL     0x680824
#define NV_PRAMDAC0_FP_HCRTC      0x680828
#define NV_PRAMDAC0_FP_HSYNC_S    0x68082c
#define NV_PRAMDAC0_FP_HSYNC_E    0x680830
#define NV_PRAMDAC0_FP_TG_CTRL    0x680848
#define NV_PRAMDAC0_FP_DEBUG_0    0x680880
#define NV_PRAMDAC0_FP_TMDS_CTRL  0x6808b0
#define NV_PRAMDAC0_FP_TMDS_DATA  0x6808b4

/* PRMDIO0 (Primary VGA DAC IO Shadow) 0x681000..0x681FFF */

/* PRAMDAC1 (Secondary Display Output Engine) 0x682000..0x682FFF */
#define NV_PRAMDAC1_BASE          0x682000

/* PRMDIO1 (Secondary VGA DAC IO Shadow) 0x683000..0x683FFF */

OBJECT_DECLARE_SIMPLE_TYPE(NV34MState, NV34M_VGA)

typedef struct NVPFBState {
    uint32_t cfg0;
    uint32_t cfg1;
    uint32_t cstatus;
    uint32_t timing0;
    uint32_t timing1;
    uint32_t timing2;
    uint32_t ram_amount;
    uint32_t refresh;
    uint32_t ram_cfg;
    uint32_t wbc;
} NVPFBState;

typedef struct NVPCRTCState {
    uint32_t intr;
    uint32_t intr_en;
    uint32_t regs[256];
} NVPCRTCState;

typedef struct NVPRAMDACState {
    uint32_t nvpll;
    uint32_t mpll;
    uint32_t vpll;
    uint32_t vpll2;
    uint32_t pll_ctrl;
    uint32_t pll_setup;
    uint32_t sel_clk;
    uint32_t nvpll_s2;
    uint32_t mpll_s2;
    uint32_t vpll_s2;
    uint32_t vpll2_s2;
    uint32_t general_ctrl;
    uint32_t regs[1024];
} NVPRAMDACState;

struct NV34MState {
    PCIDevice dev;
    VGACommonState vga;

    MemoryRegion mmio;
    MemoryRegion ramin;

    /* PMC Registers */
    uint32_t pmc_boot_0;
    uint32_t pmc_boot_1;
    uint32_t pmc_intr_0;
    uint32_t pmc_intr_en_0;
    uint32_t pmc_enable;

    /* PTIMER Registers */
    uint32_t ptimer_numerator;
    uint32_t ptimer_denominator;

    /* PBUS / Straps */
    uint32_t pbus_debug_0;
    uint32_t pbus_debug_1;
    uint32_t pstraps;

    /* PFB (Memory Controller) */
    NVPFBState pfb;

    /* PCRTC0 & PCRTC1 */
    NVPCRTCState pcrtc[2];

    /* PRAMDAC0 & PRAMDAC1 */
    NVPRAMDACState pramdac[2];

    const GraphicHwOps *default_hw_ops;
    uint8_t *ramin_buf;
};

static NV34MState *global_nv34m_state = NULL;

static inline uint32_t nv34m_get_cpu_eip(void)
{
    return pc_m200_get_cpu_eip();
}

static uint64_t nv34m_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    NV34MState *s = opaque;
    uint32_t val = 0;
    uint32_t eip = nv34m_get_cpu_eip();

    switch (addr) {
    /* PMC (0x000000..0x000FFF) */
    case NV_PMC_BOOT_0:   val = s->pmc_boot_0; break;
    case NV_PMC_BOOT_1:   val = s->pmc_boot_1; break;
    case NV_PMC_INTR_0:   val = s->pmc_intr_0; break;
    case NV_PMC_INTR_EN_0:val = s->pmc_intr_en_0; break;
    case NV_PMC_ENABLE:   val = s->pmc_enable; break;

    /* PTIMER (0x009000..0x009FFF) */
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

    /* PBUS (0x001000..0x001FFF) */
    case NV_PBUS_DEBUG_0:
        val = s->pbus_debug_0;
        break;
    case NV_PBUS_DEBUG_1:
        val = s->pbus_debug_1;
        break;
    case NV_PBUS_PCI_NV_0:
        val = (PCI_DEVICE_ID_NVIDIA_GEFORCE_FX_GO5200 << 16) | PCI_VENDOR_ID_NVIDIA;
        break;
    case NV_PBUS_PCI_NV_1:
        val = 0x02b00007;
        break;

    /* PFB (Memory Controller 0x100000..0x100FFF) */
    case NV_PFB_CFG0:
        val = s->pfb.cfg0;
        fprintf(stderr, "[NV34M PFB R] CFG0 (0x100080) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_CFG1:
        val = s->pfb.cfg1;
        fprintf(stderr, "[NV34M PFB R] CFG1 (0x100084) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_CSTATUS:
        val = s->pfb.cstatus ? s->pfb.cstatus : s->vga.vram_size;
        fprintf(stderr, "[NV34M PFB R] CSTATUS/VRAM_SIZE (0x100100) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_TIMING0:
        val = s->pfb.timing0;
        fprintf(stderr, "[NV34M PFB R] TIMING0 (0x100200) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_TIMING1:
        val = s->pfb.timing1;
        fprintf(stderr, "[NV34M PFB R] TIMING1 (0x100204) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_TIMING2:
        val = s->pfb.timing2;
        fprintf(stderr, "[NV34M PFB R] TIMING2 (0x100208) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_RAM_AMOUNT:
        val = s->pfb.ram_amount ? s->pfb.ram_amount : s->vga.vram_size;
        fprintf(stderr, "[NV34M PFB R] RAM_AMOUNT (0x10020c) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_REFRESH:
        val = s->pfb.refresh;
        fprintf(stderr, "[NV34M PFB R] REFRESH (0x100210) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_RAM_CFG:
        val = s->pfb.ram_cfg;
        fprintf(stderr, "[NV34M PFB R] RAM_CFG (0x100220) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_WBC:
        val = s->pfb.wbc;
        fprintf(stderr, "[NV34M PFB R] WBC (0x100400) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;

    /* PSTRAPS (0x101000) */
    case NV_PSTRAPS:
        val = s->pstraps;
        fprintf(stderr, "[NV34M PSTRAPS R] (0x101000) -> 0x%08x (eip=0x%08x)\n", val, eip);
        break;

    /* PCRTC0 (0x600000..0x600FFF) */
    case 0x600000 ... 0x600fff: {
        uint32_t off = addr - 0x600000;
        if (off == 0x100) val = s->pcrtc[0].intr;
        else if (off == 0x140) val = s->pcrtc[0].intr_en;
        else if ((off >> 2) < 256) val = s->pcrtc[0].regs[off >> 2];
        fprintf(stderr, "[NV34M PCRTC0 R] offset=0x%04x -> 0x%08x (eip=0x%08x)\n", off, val, eip);
        break;
    }

    /* PRMCIO0 (0x601000..0x601FFF: VGA CRTC / Sequencer / GC IO shadow) */
    case 0x601000 ... 0x601fff: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        val = vga_ioport_read(&s->vga, port);
        fprintf(stderr, "[NV34M PRMCIO0 R] port=0x%03x -> 0x%08x (eip=0x%08x)\n", port, val, eip);
        break;
    }

    /* PCRTC1 (0x602000..0x602FFF) */
    case 0x602000 ... 0x602fff: {
        uint32_t off = addr - 0x602000;
        if (off == 0x100) val = s->pcrtc[1].intr;
        else if (off == 0x140) val = s->pcrtc[1].intr_en;
        else if ((off >> 2) < 256) val = s->pcrtc[1].regs[off >> 2];
        fprintf(stderr, "[NV34M PCRTC1 R] offset=0x%04x -> 0x%08x (eip=0x%08x)\n", off, val, eip);
        break;
    }

    /* PRMCIO1 (0x603000..0x603FFF: Secondary VGA IO shadow) */
    case 0x603000 ... 0x603fff: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        val = vga_ioport_read(&s->vga, port);
        fprintf(stderr, "[NV34M PRMCIO1 R] port=0x%03x -> 0x%08x (eip=0x%08x)\n", port, val, eip);
        break;
    }

    /* PRAMDAC0 (0x680000..0x680FFF: Primary Display Output Engine) */
    case 0x680000 ... 0x680fff: {
        uint32_t off = addr - 0x680000;
        uint32_t idx = off >> 2;
        if (idx < 1024) val = s->pramdac[0].regs[idx];
        const char *sub = (off < 0x400) ? "CURSOR" :
                          (off < 0x600) ? "PLL" :
                          (off < 0x800) ? "GENERAL" : "FP";
        fprintf(stderr, "[NV34M PRAMDAC0 R] %s (0x%06"HWADDR_PRIx") -> 0x%08x (eip=0x%08x)\n",
                sub, addr, val, eip);
        break;
    }

    /* PRMDIO0 (0x681000..0x681FFF: Primary VGA DAC IO shadow) */
    case 0x681000 ... 0x681fff: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        val = vga_ioport_read(&s->vga, port);
        fprintf(stderr, "[NV34M PRMDIO0 R] port=0x%03x -> 0x%08x (eip=0x%08x)\n", port, val, eip);
        break;
    }

    /* PRAMDAC1 (0x682000..0x682FFF: Secondary Display Output Engine) */
    case 0x682000 ... 0x682fff: {
        uint32_t off = addr - 0x682000;
        uint32_t idx = off >> 2;
        if (idx < 1024) val = s->pramdac[1].regs[idx];
        const char *sub = (off < 0x400) ? "CURSOR" :
                          (off < 0x600) ? "PLL" :
                          (off < 0x800) ? "GENERAL" : "FP";
        fprintf(stderr, "[NV34M PRAMDAC1 R] %s (0x%06"HWADDR_PRIx") -> 0x%08x (eip=0x%08x)\n",
                sub, addr, val, eip);
        break;
    }

    /* PRMDIO1 (0x683000..0x683FFF: Secondary VGA DAC IO shadow) */
    case 0x683000 ... 0x683fff: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        val = vga_ioport_read(&s->vga, port);
        fprintf(stderr, "[NV34M PRMDIO1 R] port=0x%03x -> 0x%08x (eip=0x%08x)\n", port, val, eip);
        break;
    }

    /* PRAMIN (0x700000..0x7FFFFF) */
    case 0x700000 ... 0x7fffff:
        if (s->ramin_buf) {
            uint32_t offset = (addr - 0x700000) & (NV34M_RAMIN_SIZE - 1);
            memcpy(&val, &s->ramin_buf[offset], MIN(size, 4));
        }
        break;

    default:
        if (addr >= 0x100000 && addr < 0x101000) {
            fprintf(stderr, "[NV34M PFB R (unhandled)] offset=0x%04x size=%u -> 0x%08x (eip=0x%08x)\n",
                    (uint32_t)(addr - 0x100000), size, val, eip);
        } else {
            fprintf(stderr, "[NV34M MMIO READ] addr=0x%06"HWADDR_PRIx" size=%u -> 0x%08x (eip=0x%08x)\n",
                    addr, size, val, eip);
        }
        break;
    }

    return val;
}

static void nv34m_mmio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    NV34MState *s = opaque;
    uint32_t eip = nv34m_get_cpu_eip();

    switch (addr) {
    /* PMC */
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

    /* PTIMER */
    case NV_PTIMER_NUMERATOR:
        s->ptimer_numerator = val;
        break;
    case NV_PTIMER_DENOMINATOR:
        s->ptimer_denominator = val;
        break;

    /* PBUS */
    case NV_PBUS_DEBUG_0:
        s->pbus_debug_0 = val;
        break;
    case NV_PBUS_DEBUG_1:
        s->pbus_debug_1 = val;
        break;

    /* PFB (Memory Controller 0x100000..0x100FFF) */
    case NV_PFB_CFG0:
        s->pfb.cfg0 = val;
        fprintf(stderr, "[NV34M PFB W] CFG0 (0x100080) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_CFG1:
        s->pfb.cfg1 = val;
        fprintf(stderr, "[NV34M PFB W] CFG1 (0x100084) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_CSTATUS:
        s->pfb.cstatus = val;
        fprintf(stderr, "[NV34M PFB W] CSTATUS (0x100100) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_TIMING0:
        s->pfb.timing0 = val;
        fprintf(stderr, "[NV34M PFB W] TIMING0 (0x100200) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_TIMING1:
        s->pfb.timing1 = val;
        fprintf(stderr, "[NV34M PFB W] TIMING1 (0x100204) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_TIMING2:
        s->pfb.timing2 = val;
        fprintf(stderr, "[NV34M PFB W] TIMING2 (0x100208) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_RAM_AMOUNT:
        s->pfb.ram_amount = val;
        fprintf(stderr, "[NV34M PFB W] RAM_AMOUNT (0x10020c) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_REFRESH:
        s->pfb.refresh = val;
        fprintf(stderr, "[NV34M PFB W] REFRESH (0x100210) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_RAM_CFG:
        s->pfb.ram_cfg = val;
        fprintf(stderr, "[NV34M PFB W] RAM_CFG (0x100220) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;
    case NV_PFB_WBC:
        s->pfb.wbc = val;
        fprintf(stderr, "[NV34M PFB W] WBC (0x100400) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;

    /* PSTRAPS */
    case NV_PSTRAPS:
        s->pstraps = val;
        fprintf(stderr, "[NV34M PSTRAPS W] (0x101000) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        break;

    /* PCRTC0 (0x600000..0x600FFF) */
    case 0x600000 ... 0x600fff: {
        uint32_t off = addr - 0x600000;
        if (off == 0x100) s->pcrtc[0].intr &= ~((uint32_t)val);
        else if (off == 0x140) s->pcrtc[0].intr_en = val;
        else if ((off >> 2) < 256) s->pcrtc[0].regs[off >> 2] = val;
        fprintf(stderr, "[NV34M PCRTC0 W] offset=0x%04x = 0x%08"PRIx64" (eip=0x%08x)\n", off, val, eip);
        break;
    }

    /* PRMCIO0 (0x601000..0x601FFF: VGA CRTC / Sequencer / GC shadow) */
    case 0x601000 ... 0x601fff: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        vga_ioport_write(&s->vga, port, val);
        fprintf(stderr, "[NV34M PRMCIO0 W] port=0x%03x = 0x%08"PRIx64" (eip=0x%08x)\n", port, val, eip);
        break;
    }

    /* PCRTC1 (0x602000..0x602FFF) */
    case 0x602000 ... 0x602fff: {
        uint32_t off = addr - 0x602000;
        if (off == 0x100) s->pcrtc[1].intr &= ~((uint32_t)val);
        else if (off == 0x140) s->pcrtc[1].intr_en = val;
        else if ((off >> 2) < 256) s->pcrtc[1].regs[off >> 2] = val;
        fprintf(stderr, "[NV34M PCRTC1 W] offset=0x%04x = 0x%08"PRIx64" (eip=0x%08x)\n", off, val, eip);
        break;
    }

    /* PRMCIO1 (0x603000..0x603FFF: Secondary VGA IO shadow) */
    case 0x603000 ... 0x603fff: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        vga_ioport_write(&s->vga, port, val);
        fprintf(stderr, "[NV34M PRMCIO1 W] port=0x%03x = 0x%08"PRIx64" (eip=0x%08x)\n", port, val, eip);
        break;
    }

    /* PRAMDAC0 (0x680000..0x680FFF: Primary Display Output Engine) */
    case 0x680000 ... 0x680fff: {
        uint32_t off = addr - 0x680000;
        uint32_t idx = off >> 2;
        if (idx < 1024) s->pramdac[0].regs[idx] = val;
        const char *sub = (off < 0x400) ? "CURSOR" :
                          (off < 0x600) ? "PLL" :
                          (off < 0x800) ? "GENERAL" : "FP";
        fprintf(stderr, "[NV34M PRAMDAC0 W] %s (0x%06"HWADDR_PRIx") = 0x%08"PRIx64" (eip=0x%08x)\n",
                sub, addr, val, eip);
        break;
    }

    /* PRMDIO0 (0x681000..0x681FFF: Primary VGA DAC IO shadow) */
    case 0x681000 ... 0x681fff: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        vga_ioport_write(&s->vga, port, val);
        fprintf(stderr, "[NV34M PRMDIO0 W] port=0x%03x = 0x%08"PRIx64" (eip=0x%08x)\n", port, val, eip);
        break;
    }

    /* PRAMDAC1 (0x682000..0x682FFF: Secondary Display Output Engine) */
    case 0x682000 ... 0x682fff: {
        uint32_t off = addr - 0x682000;
        uint32_t idx = off >> 2;
        if (idx < 1024) s->pramdac[1].regs[idx] = val;
        const char *sub = (off < 0x400) ? "CURSOR" :
                          (off < 0x600) ? "PLL" :
                          (off < 0x800) ? "GENERAL" : "FP";
        fprintf(stderr, "[NV34M PRAMDAC1 W] %s (0x%06"HWADDR_PRIx") = 0x%08"PRIx64" (eip=0x%08x)\n",
                sub, addr, val, eip);
        break;
    }

    /* PRMDIO1 (0x683000..0x683FFF: Secondary VGA DAC IO shadow) */
    case 0x683000 ... 0x683fff: {
        uint32_t port = 0x3c0 + (addr & 0x1f);
        vga_ioport_write(&s->vga, port, val);
        fprintf(stderr, "[NV34M PRMDIO1 W] port=0x%03x = 0x%08"PRIx64" (eip=0x%08x)\n", port, val, eip);
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
        if (addr >= 0x100000 && addr < 0x101000) {
            fprintf(stderr, "[NV34M PFB W (unhandled)] offset=0x%04x val=0x%08"PRIx64" size=%u (eip=0x%08x)\n",
                    (uint32_t)(addr - 0x100000), val, size, eip);
        } else {
            fprintf(stderr, "[NV34M MMIO WRITE] addr=0x%06"HWADDR_PRIx" val=0x%08"PRIx64" size=%u (eip=0x%08x)\n",
                    addr, val, size, eip);
        }
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
    s->pbus_debug_0 = 0x00000000;
    s->pbus_debug_1 = 0x00000000;
    s->pstraps = 0x00000014; /* 32MB DDR1, 1400x1050 panel */

    memset(&s->pcrtc, 0, sizeof(s->pcrtc));
    memset(&s->pramdac, 0, sizeof(s->pramdac));

    /* Initialize PFB (Memory Controller - 32MB DDR1) */
    s->pfb.cfg0 = 0x00000000;
    s->pfb.cfg1 = 0x00000000;
    s->pfb.cstatus = s->vga.vram_size;
    s->pfb.ram_amount = s->vga.vram_size;
    s->pfb.timing0 = 0x00000000;
    s->pfb.timing1 = 0x00000000;
    s->pfb.timing2 = 0x00000000;
    s->pfb.refresh = 0x00000000;
    s->pfb.ram_cfg = 0x00000000;
    s->pfb.wbc = 0x00000000;

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
    /* Standard hardware memory routing: CPU writes directly to 0xB8000 VRAM */
}

static uint8_t *nv34m_ram_ptr = NULL;

void nv34m_set_system_ram(uint8_t *ram)
{
    nv34m_ram_ptr = ram;
}

/* VGA display update is handled by standard vga_update_display via vga_init() */

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

    /* ar_index bit 5 enables palette - required for standard text mode rendering */
    vga->ar_index = 0x20;

    s->ramin_buf = g_malloc0(NV34M_RAMIN_SIZE);

    /* Setup BAR0 (16MB MMIO) */
    memory_region_init_io(&s->mmio, OBJECT(dev), &nv34m_mmio_ops, s,
                          "nv34m-mmio", NV34M_MMIO_SIZE);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    /* Setup BAR1 (32MB VRAM Aperture) */
    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_MEM_PREFETCH, &vga->vram);

    /* BAR1 / VRAM Physical Aperture Self-Test Pattern Validation */
    if (vga->vram_ptr) {
        static const uint32_t test_offsets[] = {0x000000, 0x100000, 0x01ffff00};
        static const uint32_t test_patterns[] = {0xAA55AA55, 0x12345678, 0xDEADBEEF};
        for (int t = 0; t < 3; t++) {
            uint32_t off = test_offsets[t];
            uint32_t pat = test_patterns[t];
            uint32_t orig;
            memcpy(&orig, vga->vram_ptr + off, 4);
            memcpy(vga->vram_ptr + off, &pat, 4);
            uint32_t readback;
            memcpy(&readback, vga->vram_ptr + off, 4);
            memcpy(vga->vram_ptr + off, &orig, 4);
            if (readback == pat) {
                fprintf(stderr, "[NV34M PFB INIT] BAR1/VRAM Aperture Validation PASS at offset 0x%08x (0x%08x)\n",
                        off, pat);
            } else {
                fprintf(stderr, "[NV34M PFB INIT ERROR] BAR1/VRAM Aperture FAIL at offset 0x%08x (wrote 0x%08x, read 0x%08x)\n",
                        off, pat, readback);
            }
        }
    }

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
