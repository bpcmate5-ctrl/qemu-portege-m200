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
#define NV34M_AGP_STATUS  0x1f000017
#define NV34M_AGP_COMMAND_WMASK 0x1f000317

/* =========================================================================
 * NV34 Register Definitions (Categorized by Engine Block)
 * ========================================================================= */

/* PMC (Master Control) 0x000000..0x000FFF */
#define NV_PMC_BOOT_0             0x000000
#define NV_PMC_BOOT_1             0x000004
#define NV_PMC_INTR_0             0x000100
#define NV_PMC_INTR_EN_0          0x000140
#define NV_PMC_INTR_LINE_HOST     0x000160
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

/* Toshiba Portégé M200 Authentic VBIOS Native Panel Constants (VBIOS offset 0x9494) */
#define NV34M_NATIVE_PANEL_WIDTH       1400
#define NV34M_NATIVE_PANEL_HEIGHT      1050
#define NV34M_NATIVE_PANEL_HSYNC_START 1448
#define NV34M_NATIVE_PANEL_HSYNC_END   1560
#define NV34M_NATIVE_PANEL_HTOTAL      1688
#define NV34M_NATIVE_PANEL_VSYNC_START 1051
#define NV34M_NATIVE_PANEL_VSYNC_END   1054
#define NV34M_NATIVE_PANEL_VTOTAL      1066
#define NV34M_NATIVE_PANEL_PIXEL_CLOCK 108000000ULL /* 108.0 MHz */
#define NV34M_NATIVE_FRAME_PERIOD_NS   16661185ULL  /* 16.661185 ms -> exact 60.0197 Hz */

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
    uint32_t regs[1024];
} NVPCRTCState;

typedef struct NVPRAMDACState {
    uint32_t regs[1024];
} NVPRAMDACState;

#define PRAMDAC_REG(s, head, off) ((s)->pramdac[(head)].regs[((off) & 0xFFF) >> 2])
#define PCRTC_REG(s, head, off)   ((s)->pcrtc[(head)].regs[((off) & 0xFFF) >> 2])

struct NV34MState {
    PCIDevice dev;
    VGACommonState vga;

    MemoryRegion mmio;
    MemoryRegion ramin;
    MemoryRegion bar1_aperture;
    MemoryRegion bar1_vram;

    /* PMC Registers */
    uint32_t pmc_boot_0;
    uint32_t pmc_boot_1;
    uint32_t pmc_intr_0;
    uint32_t pmc_intr_en_0;
    uint32_t pmc_enable;
    bool pmc_sw_intr;
    bool irq_level;

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

    /* 60.0197 Hz Hardware VBLANK Timer */
    QEMUTimer *vblank_timer;

    /* Display Timing State */
    uint64_t frame_period_ns;
    uint64_t pixel_clock_hz;
    int64_t next_vblank_ns;
    uint32_t active_hdisplay;
    uint32_t active_htotal;
    uint32_t active_vdisplay;
    uint32_t active_vtotal;
    bool display_timing_valid;
    bool display_timing_dynamic;

    /* Visual Self-Test & Native NV34 Scanout Bridge State */
    bool visual_selftest;         /* Property: Legacy VGA text visual self-test */
    bool native_scanout_test;     /* Property: Native 1400x1050 linear FB visual self-test */
    bool native_scanout_active;   /* Runtime scanout mode: true = native linear FB, false = legacy VGA */
    uint32_t native_fb_offset;    /* Framebuffer VRAM start offset */
    uint32_t live_test_frame;     /* Frame counter for live update testing */

    /* Activity Counters for Milestone 2.5d Diagnostics */
    uint64_t count_b8000_writes;
    uint64_t count_a0000_writes;
    uint64_t count_bar1_writes;
    uint64_t count_pcrtc_writes;
    uint64_t count_pramdac_writes;
    uint64_t count_pmc_writes;
    uint32_t activity_report_ticks;

    const GraphicHwOps *default_hw_ops;
    uint8_t *ramin_buf;
};

static NV34MState *global_nv34m_state = NULL;

static void nv34m_render_vga_test_pattern(NV34MState *s);
static void nv34m_render_native_test_pattern(NV34MState *s, int phase);

#define NV_PMC_INTR_PCRTC0        (1u << 24)
#define NV_PMC_INTR_PCRTC1        (1u << 25)
#define NV_PMC_INTR_EN_0_HARDWARE (1u << 0)
#define NV_PMC_INTR_EN_0_SOFTWARE (1u << 1)

static void nv34m_update_irq(NV34MState *s)
{
    uint32_t hw_intr = 0;

    if (s->pcrtc[0].intr & s->pcrtc[0].intr_en) {
        hw_intr |= NV_PMC_INTR_PCRTC0;
    }
    if (s->pcrtc[1].intr & s->pcrtc[1].intr_en) {
        hw_intr |= NV_PMC_INTR_PCRTC1;
    }

    s->pmc_intr_0 = (s->pmc_sw_intr ? 0x80000000u : 0) | hw_intr;

    bool hw_irq = (s->pmc_intr_en_0 & NV_PMC_INTR_EN_0_HARDWARE) && (hw_intr != 0);
    bool sw_irq = (s->pmc_intr_en_0 & NV_PMC_INTR_EN_0_SOFTWARE) && s->pmc_sw_intr;
    s->irq_level = hw_irq || sw_irq;

    pci_set_irq(&s->dev, s->irq_level ? 1 : 0);
}

static void nv34m_decode_pll_two_stage(int head, const char *name, uint32_t s1_val, uint32_t s2_val)
{
    uint32_t m1 = s1_val & 0xFF;
    uint32_t n1 = (s1_val >> 8) & 0xFF;
    uint32_t p1 = (s1_val >> 16) & 0x07;

    uint32_t m2 = s2_val & 0xFF;
    uint32_t n2 = (s2_val >> 8) & 0xFF;
    uint32_t p2 = (s2_val >> 16) & 0x07;

    if (m1 > 0) {
        /* Reference clock is 27.0 MHz */
        double f_s1 = (27.0 * n1) / (m1 * (1 << p1));
        if (m2 > 0 && n2 > 0) {
            double f_out = (f_s1 * n2) / (m2 * (1 << p2));
            fprintf(stderr, "[NV34M 2-STAGE PLL DECODE Head%d] %s: S1(M1=%u, N1=%u, P1=%u -> %.3f MHz), S2(M2=%u, N2=%u, P2=%u) -> F_OUT=%.3f MHz\n",
                    head, name, m1, n1, p1, f_s1, m2, n2, p2, f_out);
        } else {
            fprintf(stderr, "[NV34M PLL DECODE Head%d] %s: M=%u, N=%u, P=%u (raw=0x%08x) -> F_OUT=%.3f MHz\n",
                    head, name, m1, n1, p1, s1_val, f_s1);
        }
    }
}

static void nv34m_decode_fp(int head, uint32_t off, uint32_t val)
{
    const char *reg_name = NULL;
    switch (off) {
    case 0x800: reg_name = "FP_VDISPLAY_END"; break;
    case 0x804: reg_name = "FP_VTOTAL"; break;
    case 0x808: reg_name = "FP_VCRTC"; break;
    case 0x80c: reg_name = "FP_VSYNC_START"; break;
    case 0x810: reg_name = "FP_VSYNC_END"; break;
    case 0x820: reg_name = "FP_HDISPLAY_END"; break;
    case 0x824: reg_name = "FP_HTOTAL"; break;
    case 0x828: reg_name = "FP_HCRTC"; break;
    case 0x82c: reg_name = "FP_HSYNC_START"; break;
    case 0x830: reg_name = "FP_HSYNC_END"; break;
    case 0x848: reg_name = "FP_TG_CTRL"; break;
    case 0x880: reg_name = "FP_DEBUG_0"; break;
    case 0x8b0: reg_name = "FP_TMDS_CTRL"; break;
    case 0x8b4: reg_name = "FP_TMDS_DATA"; break;
    default:    reg_name = "FP_OTHER"; break;
    }
    fprintf(stderr, "[NV34M FP TIMING Head%d] %s (0x%04x) = 0x%08x (%u dec)\n",
            head, reg_name, off, val, val);
}

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
    case NV_PMC_BOOT_0:        val = s->pmc_boot_0; break;
    case NV_PMC_BOOT_1:        val = s->pmc_boot_1; break;
    case NV_PMC_INTR_0:        val = s->pmc_intr_0; break;
    case NV_PMC_INTR_EN_0:     val = s->pmc_intr_en_0; break;
    case NV_PMC_INTR_LINE_HOST:val = s->irq_level ? 0 : 1; break;
    case NV_PMC_ENABLE:        val = s->pmc_enable; break;

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
        uint32_t idx = off >> 2;
        if (off == 0x100) val = s->pcrtc[0].intr;
        else if (off == 0x140) val = s->pcrtc[0].intr_en;
        else if (idx < ARRAY_SIZE(s->pcrtc[0].regs)) val = s->pcrtc[0].regs[idx];
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
        uint32_t idx = off >> 2;
        if (off == 0x100) val = s->pcrtc[1].intr;
        else if (off == 0x140) val = s->pcrtc[1].intr_en;
        else if (idx < ARRAY_SIZE(s->pcrtc[1].regs)) val = s->pcrtc[1].regs[idx];
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
        if (idx < ARRAY_SIZE(s->pramdac[0].regs)) val = s->pramdac[0].regs[idx];
        const char *sub = (off >= 0x300 && off <= 0x33f) ? "CURSOR" :
                          (off >= 0x500 && off <= 0x5ff) ? "PLL" :
                          (off >= 0x600 && off <= 0x7ff) ? "GENERAL" :
                          (off >= 0x800 && off <= 0x8ff) ? "FP" : "OTHER";
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
        if (idx < ARRAY_SIZE(s->pramdac[1].regs)) val = s->pramdac[1].regs[idx];
        const char *sub = (off >= 0x300 && off <= 0x33f) ? "CURSOR" :
                          (off >= 0x500 && off <= 0x5ff) ? "PLL" :
                          (off >= 0x600 && off <= 0x7ff) ? "GENERAL" :
                          (off >= 0x800 && off <= 0x8ff) ? "FP" : "OTHER";
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
        s->pmc_intr_0 &= ~((uint32_t)val & 0x80000000u); /* bit 31 software interrupt is W1C */
        nv34m_update_irq(s);
        break;
    case NV_PMC_INTR_EN_0:
        s->pmc_intr_en_0 = val;
        nv34m_update_irq(s);
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
        uint32_t idx = off >> 2;
        s->count_pcrtc_writes++;
        if (off == 0x100) {
            s->pcrtc[0].intr &= ~((uint32_t)val);
            nv34m_update_irq(s);
        } else if (off == 0x140) {
            s->pcrtc[0].intr_en = val;
            nv34m_update_irq(s);
        } else if (idx < ARRAY_SIZE(s->pcrtc[0].regs)) {
            s->pcrtc[0].regs[idx] = val;
        }
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
        uint32_t idx = off >> 2;
        if (off == 0x100) {
            s->pcrtc[1].intr &= ~((uint32_t)val);
            nv34m_update_irq(s);
        } else if (off == 0x140) {
            s->pcrtc[1].intr_en = val;
            nv34m_update_irq(s);
        } else if (idx < ARRAY_SIZE(s->pcrtc[1].regs)) {
            s->pcrtc[1].regs[idx] = val;
        }
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
        if (idx < ARRAY_SIZE(s->pramdac[0].regs)) s->pramdac[0].regs[idx] = val;
        if (off == 0x500 || off == 0x570)
            nv34m_decode_pll_two_stage(0, "NVPLL (Core)", PRAMDAC_REG(s, 0, 0x500), PRAMDAC_REG(s, 0, 0x570));
        else if (off == 0x504 || off == 0x574)
            nv34m_decode_pll_two_stage(0, "MPLL (Memory)", PRAMDAC_REG(s, 0, 0x504), PRAMDAC_REG(s, 0, 0x574));
        else if (off == 0x508 || off == 0x578)
            nv34m_decode_pll_two_stage(0, "VPLL (Pixel Clock)", PRAMDAC_REG(s, 0, 0x508), PRAMDAC_REG(s, 0, 0x578));
        else if (off == 0x520 || off == 0x57c)
            nv34m_decode_pll_two_stage(0, "VPLL2", PRAMDAC_REG(s, 0, 0x520), PRAMDAC_REG(s, 0, 0x57c));
        else if (off == 0x524)
            fprintf(stderr, "[NV34M CLK SEL Head0] SEL_CLK (0x680524) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        else if (off >= 0x800 && off <= 0x8ff)
            nv34m_decode_fp(0, off, (uint32_t)val);

        const char *sub = (off >= 0x300 && off <= 0x33f) ? "CURSOR" :
                          (off >= 0x500 && off <= 0x5ff) ? "PLL" :
                          (off >= 0x600 && off <= 0x7ff) ? "GENERAL" :
                          (off >= 0x800 && off <= 0x8ff) ? "FP" : "OTHER";
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
        if (idx < ARRAY_SIZE(s->pramdac[1].regs)) s->pramdac[1].regs[idx] = val;
        if (off == 0x500 || off == 0x570)
            nv34m_decode_pll_two_stage(1, "NVPLL (Core)", PRAMDAC_REG(s, 1, 0x500), PRAMDAC_REG(s, 1, 0x570));
        else if (off == 0x504 || off == 0x574)
            nv34m_decode_pll_two_stage(1, "MPLL (Memory)", PRAMDAC_REG(s, 1, 0x504), PRAMDAC_REG(s, 1, 0x574));
        else if (off == 0x508 || off == 0x578)
            nv34m_decode_pll_two_stage(1, "VPLL (Pixel Clock)", PRAMDAC_REG(s, 1, 0x508), PRAMDAC_REG(s, 1, 0x578));
        else if (off == 0x520 || off == 0x57c)
            nv34m_decode_pll_two_stage(1, "VPLL2", PRAMDAC_REG(s, 1, 0x520), PRAMDAC_REG(s, 1, 0x57c));
        else if (off == 0x524)
            fprintf(stderr, "[NV34M CLK SEL Head1] SEL_CLK (0x682524) = 0x%08"PRIx64" (eip=0x%08x)\n", val, eip);
        else if (off >= 0x800 && off <= 0x8ff)
            nv34m_decode_fp(1, off, (uint32_t)val);

        const char *sub = (off >= 0x300 && off <= 0x33f) ? "CURSOR" :
                          (off >= 0x500 && off <= 0x5ff) ? "PLL" :
                          (off >= 0x600 && off <= 0x7ff) ? "GENERAL" :
                          (off >= 0x800 && off <= 0x8ff) ? "FP" : "OTHER";
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
    nv34m_update_irq(s);

    /* Initialize Display Timing State (1400x1050 @ 60.0197 Hz Native Panel) */
    s->frame_period_ns = NV34M_NATIVE_FRAME_PERIOD_NS;
    s->pixel_clock_hz = NV34M_NATIVE_PANEL_PIXEL_CLOCK;
    s->active_hdisplay = NV34M_NATIVE_PANEL_WIDTH;
    s->active_htotal = NV34M_NATIVE_PANEL_HTOTAL;
    s->active_vdisplay = NV34M_NATIVE_PANEL_HEIGHT;
    s->active_vtotal = NV34M_NATIVE_PANEL_VTOTAL;
    s->display_timing_valid = true;
    s->display_timing_dynamic = false;
    s->next_vblank_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + s->frame_period_ns;

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

    if (s->visual_selftest) {
        nv34m_render_vga_test_pattern(s);
    }
    if (s->native_scanout_test) {
        nv34m_render_native_test_pattern(s, 0);
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

static void nv34m_render_vga_test_pattern(NV34MState *s)
{
    VGACommonState *vga = &s->vga;
    if (!vga->vram_ptr) return;

    /* Fill background with dark blue attribute (0x17) */
    for (int r = 0; r < 25; r++) {
        for (int c = 0; c < 80; c++) {
            nv34m_vga_write_text(r, c, ' ', 0x17);
        }
    }

    static const struct {
        int row;
        int col;
        const char *text;
        uint8_t attr;
    } test_lines[] = {
        {2,  0, "================================================================================", 0x1F},
        {4, 28, "NV34M VGA VISUAL TEST", 0x1E},
        {6, 33, "PORTEGE M200", 0x1F},
        {8, 29, "LEGACY VGA OUTPUT: OK", 0x2F},
        {10, 24, "1400x1050 NATIVE PANEL DISPLAY", 0x1B},
        {12, 29, "60.0197 Hz (16.661 ms)", 0x1A},
        {14, 0, "================================================================================", 0x1F},
    };

    for (size_t i = 0; i < G_N_ELEMENTS(test_lines); i++) {
        const char *p = test_lines[i].text;
        int col = test_lines[i].col;
        while (*p && col < 80) {
            nv34m_vga_write_text(test_lines[i].row, col++, *p++, test_lines[i].attr);
        }
    }
}

static void nv34m_render_native_test_pattern(NV34MState *s, int phase)
{
    VGACommonState *vga = &s->vga;
    if (!vga->vram_ptr) return;

    uint32_t width = s->active_hdisplay ? s->active_hdisplay : NV34M_NATIVE_PANEL_WIDTH;
    uint32_t height = s->active_vdisplay ? s->active_vdisplay : NV34M_NATIVE_PANEL_HEIGHT;
    uint32_t *fb = (uint32_t *)(vga->vram_ptr + s->native_fb_offset);

    static const uint32_t rgb_bars[3][3] = {
        {0x00FF0000, 0x0000FF00, 0x000000FF}, /* Frame A: RED | GREEN | BLUE */
        {0x000000FF, 0x00FF0000, 0x0000FF00}, /* Frame B: BLUE | RED | GREEN */
        {0x0000FF00, 0x000000FF, 0x00FF0000}, /* Frame C: GREEN | BLUE | RED */
    };
    const uint32_t *current_bars = rgb_bars[phase % 3];

    static const uint32_t smpte_colors[8] = {
        0x00FFFFFF, /* White */
        0x00FFFF00, /* Yellow */
        0x0000FFFF, /* Cyan */
        0x0000FF00, /* Green */
        0x00FF00FF, /* Magenta */
        0x00FF0000, /* Red */
        0x000000FF, /* Blue */
        0x00000000, /* Black */
    };

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t color = 0;
            if (y < 350) {
                /* Top third: 3 Primary RGB Bars (cycles in phase A/B/C for live update validation) */
                int bar_idx = (x * 3) / width;
                color = current_bars[bar_idx < 3 ? bar_idx : 2];
            } else if (y < 700) {
                /* Middle third: SMPTE Color Bars with Checkerboard overlay */
                if ((x < 32 || x >= (width - 32)) || ((x / 32 + y / 32) & 1)) {
                    int bar_idx = (x * 8) / width;
                    color = smpte_colors[bar_idx < 8 ? bar_idx : 7];
                } else {
                    color = ((x / 32 + y / 32) & 1) ? 0x00333333 : 0x00CCCCCC;
                }
            } else {
                /* Bottom third: Smooth horizontal/vertical gradient */
                uint32_t grad_r = (x * 255) / width;
                uint32_t grad_g = ((height - y) * 255) / 350;
                uint32_t grad_b = ((x + y) * 128) / (width + height);
                color = (grad_r << 16) | (grad_g << 8) | grad_b;
            }
            fb[y * width + x] = color;
        }
    }
}

static void nv34m_gfx_update(void *opaque)
{
    NV34MState *s = opaque;
    VGACommonState *vga = &s->vga;

    if (s->native_scanout_active || s->native_scanout_test) {
        uint32_t width = s->active_hdisplay ? s->active_hdisplay : NV34M_NATIVE_PANEL_WIDTH;
        uint32_t height = s->active_vdisplay ? s->active_vdisplay : NV34M_NATIVE_PANEL_HEIGHT;
        uint32_t pitch = width * 4;
        uint32_t offset = s->native_fb_offset;
        DisplaySurface *surface = qemu_console_surface(vga->con);

        if (!surface || surface_width(surface) != width || surface_height(surface) != height ||
            surface_data(surface) != (vga->vram_ptr + offset)) {
            surface = qemu_create_displaysurface_from(width, height,
                                                      PIXMAN_x8r8g8b8,
                                                      pitch,
                                                      vga->vram_ptr + offset);
            dpy_gfx_replace_surface(vga->con, surface);
            dpy_gfx_update_full(vga->con);
        } else {
            dpy_gfx_update(vga->con, 0, 0, width, height);
        }
    } else {
        /* Standard Legacy VGA Pipeline */
        if (s->default_hw_ops && s->default_hw_ops->gfx_update) {
            s->default_hw_ops->gfx_update(vga);
        }
    }
}

static void nv34m_gfx_invalidate(void *opaque)
{
    NV34MState *s = opaque;
    VGACommonState *vga = &s->vga;
    if (!s->native_scanout_active && !s->native_scanout_test &&
        s->default_hw_ops && s->default_hw_ops->invalidate) {
        s->default_hw_ops->invalidate(vga);
    }
}

static const GraphicHwOps nv34m_graphic_ops = {
    .gfx_update = nv34m_gfx_update,
    .invalidate = nv34m_gfx_invalidate,
};

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

static uint8_t ar_flip_flop = 0;
static uint8_t ar_index = 0;
static uint8_t dac_write_index = 0;
static uint8_t dac_rgb_state = 0; /* 0=R, 1=G, 2=B */
static uint8_t dac_cur_r = 0, dac_cur_g = 0;
static uint8_t seq_idx = 0;
static uint8_t gc_idx = 0;
static uint8_t crtc_idx = 0;

void nv34m_dump_vga_state_snapshot(const char *reason)
{
    if (!global_nv34m_state) return;
    VGACommonState *vga = &global_nv34m_state->vga;
    uint32_t eip = nv34m_get_cpu_eip();

    fprintf(stderr, "\n======================================================\n");
    fprintf(stderr, "=== FULL VGA STATE SNAPSHOT [%s] ===\n", reason);
    fprintf(stderr, "=== EIP = 0x%08x | Mode: %s (graphic_mode=%d) ===\n",
            eip, vga->graphic_mode ? "GRAPHIC" : "TEXT", vga->graphic_mode);
    fprintf(stderr, "MISC Output: 0x%02x\n", vga->msr);
    fprintf(stderr, "SEQ:  00=%02x 01=%02x 02(MapMask)=%02x 03=%02x 04(MemMode)=%02x\n",
            vga->sr[0], vga->sr[1], vga->sr[2], vga->sr[3], vga->sr[4]);
    fprintf(stderr, "GC:   00(SetReset)=%02x 01(EnSetReset)=%02x 02(ColorComp)=%02x 03(DataRotate)=%02x\n"
                    "      04(ReadMapSel)=%02x 05(Mode)=%02x 06(Misc)=%02x 07(ColorDontCare)=%02x 08(BitMask)=%02x\n",
            vga->gr[0], vga->gr[1], vga->gr[2], vga->gr[3],
            vga->gr[4], vga->gr[5], vga->gr[6], vga->gr[7], vga->gr[8]);
    fprintf(stderr, "CRTC: 00..07: %02x %02x %02x %02x %02x %02x %02x %02x\n"
                    "      08..0F: %02x %02x %02x %02x %02x %02x %02x %02x\n"
                    "      10..18: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            vga->cr[0], vga->cr[1], vga->cr[2], vga->cr[3], vga->cr[4], vga->cr[5], vga->cr[6], vga->cr[7],
            vga->cr[8], vga->cr[9], vga->cr[10], vga->cr[11], vga->cr[12], vga->cr[13], vga->cr[14], vga->cr[15],
            vga->cr[16], vga->cr[17], vga->cr[18], vga->cr[19], vga->cr[20], vga->cr[21], vga->cr[22], vga->cr[23], vga->cr[24]);
    fprintf(stderr, "ATTR: 00..0F(Palette): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n"
                    "      10(Mode)=%02x 11(Overscan)=%02x 12(PlaneEn)=%02x 13(PELPan)=%02x 14(ColorSel)=%02x\n",
            vga->ar[0], vga->ar[1], vga->ar[2], vga->ar[3], vga->ar[4], vga->ar[5], vga->ar[6], vga->ar[7],
            vga->ar[8], vga->ar[9], vga->ar[10], vga->ar[11], vga->ar[12], vga->ar[13], vga->ar[14], vga->ar[15],
            vga->ar[16], vga->ar[17], vga->ar[18], vga->ar[19], vga->ar[20]);
    fprintf(stderr, "======================================================\n\n");
}

void nv34m_log_vga_mem_write(hwaddr addr, unsigned size)
{
    if (!global_nv34m_state) return;
    if (addr < 0x10000) {
        if (global_nv34m_state->count_a0000_writes == 0) {
            nv34m_dump_vga_state_snapshot("FIRST A0000 WRITE");
        }
        global_nv34m_state->count_a0000_writes++;
    } else if (addr >= 0x18000) {
        if (global_nv34m_state->count_b8000_writes == 0) {
            nv34m_dump_vga_state_snapshot("FIRST B8000 WRITE");
        }
        global_nv34m_state->count_b8000_writes++;
    }
}

void nv34m_log_vga_ioport_read(uint32_t addr, uint32_t val)
{
    if (addr == 0x3da || addr == 0x3ba) {
        ar_flip_flop = 0;
    }
}

void nv34m_log_vga_ioport_write(uint32_t addr, uint32_t val)
{
    switch (addr) {
    case 0x3c2:
        fprintf(stderr, "[VGA MISC W] val=0x%02x (clk=%u, io_addr=%s, ram_en=%u)\n",
                val, (val >> 2) & 3, (val & 1) ? "0x3Dx" : "0x3Bx", (val >> 1) & 1);
        break;

    /* Sequencer */
    case 0x3c4:
        seq_idx = val & 0x1f;
        break;
    case 0x3c5:
        fprintf(stderr, "[VGA SEQ W] index=0x%02x val=0x%02x\n", seq_idx, val);
        break;

    /* Graphics Controller */
    case 0x3ce:
        gc_idx = val & 0x0f;
        break;
    case 0x3cf:
        fprintf(stderr, "[VGA GC W] index=0x%02x val=0x%02x\n", gc_idx, val);
        break;

    /* CRTC */
    case 0x3d4:
    case 0x3b4:
        crtc_idx = val & 0x7f;
        break;
    case 0x3d5:
    case 0x3b5:
        fprintf(stderr, "[VGA CRTC W] index=0x%02x val=0x%02x\n", crtc_idx, val);
        break;

    /* Attribute Controller */
    case 0x3c0:
        if (ar_flip_flop == 0) {
            ar_index = val & 0x1f;
            uint8_t video_en = (val >> 5) & 1;
            fprintf(stderr, "[VGA ATTR INDEX] index=0x%02x (VideoEn=%u)\n", ar_index, video_en);
            ar_flip_flop = 1;
        } else {
            fprintf(stderr, "[VGA ATTR DATA] index=0x%02x val=0x%02x\n", ar_index, val);
            ar_flip_flop = 0;
        }
        break;

    /* DAC */
    case 0x3c6:
        fprintf(stderr, "[VGA DAC MASK] val=0x%02x\n", val);
        break;
    case 0x3c7:
        break;
    case 0x3c8:
        dac_write_index = val;
        dac_rgb_state = 0;
        break;
    case 0x3c9:
        if (dac_rgb_state == 0) {
            dac_cur_r = val;
            dac_rgb_state = 1;
        } else if (dac_rgb_state == 1) {
            dac_cur_g = val;
            dac_rgb_state = 2;
        } else {
            uint8_t dac_cur_b = val;
            if (dac_write_index < 16) {
                fprintf(stderr, "[VGA DAC PALETTE] idx=%2u -> R=0x%02x G=0x%02x B=0x%02x\n",
                        dac_write_index, dac_cur_r, dac_cur_g, dac_cur_b);
            }
            dac_write_index++;
            dac_rgb_state = 0;
        }
        break;
    }
}

#define NV_PCRTC_INTR_VBLANK 0x00000001

static void nv34m_vblank_timer_cb(void *opaque)
{
    NV34MState *s = opaque;
    s->pcrtc[0].intr |= NV_PCRTC_INTR_VBLANK;
    s->pcrtc[1].intr |= NV_PCRTC_INTR_VBLANK;
    nv34m_update_irq(s);

    /* If live update test is active, cycle pattern every 60 frames (~1 sec) */
    if (s->native_scanout_test) {
        s->live_test_frame++;
        if ((s->live_test_frame % 60) == 0) {
            nv34m_render_native_test_pattern(s, s->live_test_frame / 60);
            if (s->vga.con) {
                dpy_gfx_update(s->vga.con, 0, 0,
                               s->active_hdisplay ? s->active_hdisplay : NV34M_NATIVE_PANEL_WIDTH,
                               s->active_vdisplay ? s->active_vdisplay : NV34M_NATIVE_PANEL_HEIGHT);
            }
        }
    }

    s->activity_report_ticks++;
    if ((s->activity_report_ticks % 60) == 0 &&
        (s->count_b8000_writes || s->count_a0000_writes || s->count_bar1_writes || s->count_pcrtc_writes || s->count_pramdac_writes)) {
        fprintf(stderr, "[NV34M DISPLAY ACTIVITY] B8000 writes=%"PRIu64" | A0000 writes=%"PRIu64" | BAR1 writes=%"PRIu64" | PCRTC writes=%"PRIu64" | PRAMDAC writes=%"PRIu64"\n",
                s->count_b8000_writes, s->count_a0000_writes, s->count_bar1_writes,
                s->count_pcrtc_writes, s->count_pramdac_writes);
    }

    if (s->frame_period_ns == 0) {
        s->frame_period_ns = NV34M_NATIVE_FRAME_PERIOD_NS;
    }
    s->next_vblank_ns += s->frame_period_ns;
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    if (s->next_vblank_ns <= now) {
        s->next_vblank_ns = now + s->frame_period_ns;
    }
    timer_mod(s->vblank_timer, s->next_vblank_ns);
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

    vga->con = graphic_console_init(DEVICE(dev), 0, &nv34m_graphic_ops, s);

    /* Initialize Display Timing State (1400x1050 @ 60.0197 Hz Native Panel) */
    s->frame_period_ns = NV34M_NATIVE_FRAME_PERIOD_NS;
    s->pixel_clock_hz = NV34M_NATIVE_PANEL_PIXEL_CLOCK;
    s->active_hdisplay = NV34M_NATIVE_PANEL_WIDTH;
    s->active_htotal = NV34M_NATIVE_PANEL_HTOTAL;
    s->active_vdisplay = NV34M_NATIVE_PANEL_HEIGHT;
    s->active_vtotal = NV34M_NATIVE_PANEL_VTOTAL;
    s->display_timing_valid = true;
    s->display_timing_dynamic = false;
    s->next_vblank_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + s->frame_period_ns;

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
    for (int i = 0; i < 16; i++) {
        vga->ar[i] = i;
    }
    static const uint8_t default_vga_palette[16 * 3] = {
        0x00, 0x00, 0x00,  0x00, 0x00, 0x2a,  0x00, 0x2a, 0x00,  0x00, 0x2a, 0x2a,
        0x2a, 0x00, 0x00,  0x2a, 0x00, 0x2a,  0x2a, 0x15, 0x00,  0x2a, 0x2a, 0x2a,
        0x15, 0x15, 0x15,  0x15, 0x15, 0x3f,  0x15, 0x3f, 0x15,  0x15, 0x3f, 0x3f,
        0x3f, 0x15, 0x15,  0x3f, 0x15, 0x3f,  0x3f, 0x3f, 0x15,  0x3f, 0x3f, 0x3f,
    };
    memcpy(vga->palette, default_vga_palette, sizeof(default_vga_palette));

    s->ramin_buf = g_malloc0(NV34M_RAMIN_SIZE);

    /* Setup BAR0 (16MB MMIO) */
    memory_region_init_io(&s->mmio, OBJECT(dev), &nv34m_mmio_ops, s,
                          "nv34m-mmio", NV34M_MMIO_SIZE);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    /*
     * NV34M exposes a 256 MiB prefetchable BAR1 while this board has 32 MiB
     * of physical VRAM.  Map the implemented VRAM into the first 32 MiB;
     * the aliasing/decoding of the remaining aperture is undocumented and
     * intentionally left unassigned rather than inventing wraparound.
     */
    memory_region_init(&s->bar1_aperture, OBJECT(dev),
                       "nv34m-bar1-aperture", 256 * MiB);
    memory_region_init_alias(&s->bar1_vram, OBJECT(dev),
                             "nv34m-bar1-vram", &vga->vram, 0, 32 * MiB);
    memory_region_add_subregion(&s->bar1_aperture, 0, &s->bar1_vram);
    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_MEM_PREFETCH,
                     &s->bar1_aperture);

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

    /* Physical M200 NV34M subsystem ID: Toshiba 1179:0010. */
    pci_set_word(dev->config + PCI_SUBSYSTEM_VENDOR_ID, 0x1179);
    pci_set_word(dev->config + PCI_SUBSYSTEM_ID, 0x0010);

    /* Physical NV34M capability chain: PM at 60h -> AGP at 44h. */
    if (pci_add_capability(dev, PCI_CAP_ID_AGP, 0x44, 12, errp) < 0) {
        return;
    }
    pci_set_word(dev->config + 0x46, 0x0030);
    pci_set_long(dev->config + 0x48, NV34M_AGP_STATUS);
    pci_set_long(dev->wmask + 0x4c, NV34M_AGP_COMMAND_WMASK);
    if (pci_pm_init(dev, 0x60, errp) < 0) {
        return;
    }
    pci_set_word(dev->config + 0x62, 0x0002);

    /* Interrupt routing */
    pci_config_set_interrupt_pin(dev->config, 1); /* Pin A */

    /* 60.0197 Hz Hardware VBLANK Timer (~16.661 ms per authentic M200 VBIOS) */
    s->vblank_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, nv34m_vblank_timer_cb, s);
    timer_mod(s->vblank_timer, s->next_vblank_ns);
}

static void nv34m_exit(PCIDevice *dev)
{
    NV34MState *s = NV34M_VGA(dev);
    if (s->vblank_timer) {
        timer_free(s->vblank_timer);
        s->vblank_timer = NULL;
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

static const Property nv34m_properties[] = {
    DEFINE_PROP_BOOL("visual-selftest", NV34MState, visual_selftest, false),
    DEFINE_PROP_BOOL("native-scanout-test", NV34MState, native_scanout_test, false),
};

static uint32_t nv34m_pci_read_config(PCIDevice *d, uint32_t addr, int len)
{
    uint32_t val = pci_default_read_config(d, addr, len);
    fprintf(stderr, "[NV34M PCI PROBE R] 01:00.0 offset=0x%02x len=%d -> 0x%x (eip=0x%08x)\n",
            addr, len, val, pc_m200_get_cpu_eip());
    return val;
}

static void nv34m_pci_write_config(PCIDevice *d, uint32_t addr, uint32_t val, int len)
{
    fprintf(stderr, "[NV34M PCI CFG W] 01:00.0 offset=0x%02x val=0x%x len=%d (eip=0x%08x)\n",
            addr, val, len, pc_m200_get_cpu_eip());
    pci_default_write_config(d, addr, val, len);
}

static void nv34m_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = nv34m_realize;
    k->exit = nv34m_exit;
    k->config_read = nv34m_pci_read_config;
    k->config_write = nv34m_pci_write_config;
    k->romfile = "nv34m_vbios.bin";
    k->vendor_id = PCI_VENDOR_ID_NVIDIA;
    k->device_id = PCI_DEVICE_ID_NVIDIA_GEFORCE_FX_GO5200;
    k->revision = 0xa1;
    k->class_id = PCI_CLASS_DISPLAY_VGA;
    dc->vmsd = &vmstate_nv34m;
    device_class_set_props(dc, nv34m_properties);
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
