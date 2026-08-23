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
}

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
