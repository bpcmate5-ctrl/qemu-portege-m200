/*
 * ICH4M ACPI implementation
 *
 * Copyright (c) 2024
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/isa/apm.h"
#include "hw/pci/pci.h"
#include "hw/core/qdev-properties.h"
#include "hw/acpi/acpi.h"
#include "hw/acpi/ich4m.h"
#include "hw/southbridge/ich4m.h"
#include "system/runstate.h"
#include "system/system.h"
#include "qapi/error.h"
#include "qemu/range.h"
#include "migration/vmstate.h"

#define ACPI_ENABLE 0xf1
#define ACPI_DISABLE 0xf0

static void pm_tmr_timer(ACPIREGS *ar)
{
    ICH4MLPCPMRegs *s = container_of(ar, ICH4MLPCPMRegs, acpi_regs);
    acpi_update_sci(&s->acpi_regs, s->irq);
}

static void ich4m_pm_periodic_timer_expired(void *opaque)
{
    ICH4MLPCPMRegs *pm = opaque;
    pm->smi_sts |= ICH4M_PMIO_SMI_STS_PERIODIC_STS;
    if ((pm->smi_en & (ICH4M_PMIO_SMI_EN_GBL_SMI_EN |
                       ICH4M_PMIO_SMI_EN_PERIODIC_EN)) ==
        (ICH4M_PMIO_SMI_EN_GBL_SMI_EN |
         ICH4M_PMIO_SMI_EN_PERIODIC_EN)) {
        if (getenv("M200_SMI_TRACE")) {
            fprintf(stderr,
                    "[ICH4M PERIODIC SMI] EN=%08x STS=%08x assert=1\n",
                    pm->smi_en, pm->smi_sts);
        }
        ich4m_generate_smi();
    }
    ich4m_pm_update_periodic_timer(pm);
}

void ich4m_pm_update_periodic_timer(ICH4MLPCPMRegs *pm)
{
    ICH4MLPCState *lpc = container_of(pm, ICH4MLPCState, pm);
    unsigned sel = pci_get_word(lpc->d.config + ICH4M_LPC_GEN_PMCON_1) & 3;
    int64_t period_ns = 8LL * (1 << (3 - sel)) * NANOSECONDS_PER_SECOND;

    if (pm->smi_en & ICH4M_PMIO_SMI_EN_PERIODIC_EN) {
        timer_mod(pm->periodic_timer,
                  qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + period_ns);
    } else {
        timer_del(pm->periodic_timer);
    }
}

/* apm_ctrl_changed is handled in lpc_ich4m.c */

/* This function is not used - ich4m_pm_iospace_update is used instead */

static uint64_t ich4m_gpe_readb(void *opaque, hwaddr addr, unsigned width)
{
    ICH4MLPCPMRegs *pm = opaque;
    return acpi_gpe_ioport_readb(&pm->acpi_regs, addr);
}

static void ich4m_gpe_writeb(void *opaque, hwaddr addr, uint64_t val,
                            unsigned width)
{
    ICH4MLPCPMRegs *pm = opaque;
    acpi_gpe_ioport_writeb(&pm->acpi_regs, addr, val);
    acpi_update_sci(&pm->acpi_regs, pm->irq);
}

static const MemoryRegionOps ich4m_gpe_ops = {
    .read = ich4m_gpe_readb,
    .write = ich4m_gpe_writeb,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .impl.min_access_size = 1,
    .impl.max_access_size = 1,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t ich4m_smi_read(void *opaque, hwaddr addr, unsigned width)
{
    ICH4MLPCPMRegs *pm = opaque;
    uint32_t value;
    unsigned shift;

    if (addr < 4 && addr + width <= 4) {
        value = pm->smi_en;
        shift = addr * 8;
    } else if (addr >= 4 && addr + width <= 8) {
        value = pm->smi_sts;
        shift = (addr - 4) * 8;
    } else {
        return 0;
    }

    return extract64(value, shift, width * 8);
}

static void ich4m_smi_write(void *opaque, hwaddr addr, uint64_t val,
                            unsigned width)
{
    ICH4MLPCPMRegs *pm = opaque;
    uint32_t mask;
    unsigned shift;

    if (addr < 4 && addr + width <= 4) {
        shift = addr * 8;
        mask = MAKE_64BIT_MASK(shift, width * 8);
        pm->smi_en = (pm->smi_en & ~mask) | (((uint32_t)val << shift) & mask);
        ich4m_pm_update_periodic_timer(pm);
    } else if (addr >= 4 && addr + width <= 8) {
        /* SMI_STS is write-one-to-clear. */
        shift = (addr - 4) * 8;
        mask = MAKE_64BIT_MASK(shift, width * 8);
        pm->smi_sts &= ~(((uint32_t)val << shift) & mask);
    }
    if (getenv("M200_SMI_TRACE")) {
        fprintf(stderr, "[ICH4M SMI REG] off=%02" HWADDR_PRIx
                " write=%08" PRIx64 " EN=%08x STS=%08x\n",
                addr + ICH4M_PMIO_SMI_EN, val, pm->smi_en, pm->smi_sts);
    }
}

static const MemoryRegionOps ich4m_smi_ops = {
    .read = ich4m_smi_read,
    .write = ich4m_smi_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .impl.min_access_size = 1,
    .impl.max_access_size = 4,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int ich4m_pm_post_load(void *opaque, int version_id)
{
    ICH4MLPCPMRegs *pm = opaque;
    uint32_t pm_io_base = pm->pm_io_base;
    pm->pm_io_base = 0;
    ich4m_pm_iospace_update(pm, pm_io_base);
    return 0;
}

/* Save/restore minimal ACPI state for ICH4M (similar to ich9, but without SMI). */
#define VMSTATE_GPE_ARRAY(_field, _state)                            \
 {                                                                   \
     .name       = (stringify(_field)),                              \
     .version_id = 0,                                                \
     .num        = ICH4M_PMIO_GPE0_LEN,                              \
     .info       = &vmstate_info_uint8,                              \
     .size       = sizeof(uint8_t),                                  \
     .flags      = VMS_ARRAY | VMS_POINTER,                          \
     .offset     = vmstate_offset_pointer(_state, _field, uint8_t),  \
 }

const VMStateDescription vmstate_ich4m_pm = {
    .name = "ich4m_pm",
    .version_id = 2,
    .minimum_version_id = 1,
    .post_load = ich4m_pm_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT16(acpi_regs.pm1.evt.sts, ICH4MLPCPMRegs),
        VMSTATE_UINT16(acpi_regs.pm1.evt.en, ICH4MLPCPMRegs),
        VMSTATE_UINT16(acpi_regs.pm1.cnt.cnt, ICH4MLPCPMRegs),
        VMSTATE_TIMER_PTR(acpi_regs.tmr.timer, ICH4MLPCPMRegs),
        VMSTATE_INT64(acpi_regs.tmr.overflow_time, ICH4MLPCPMRegs),
        VMSTATE_GPE_ARRAY(acpi_regs.gpe.sts, ICH4MLPCPMRegs),
        VMSTATE_GPE_ARRAY(acpi_regs.gpe.en, ICH4MLPCPMRegs),
        VMSTATE_UINT32(pm_io_base, ICH4MLPCPMRegs),
        VMSTATE_UINT32(smi_en, ICH4MLPCPMRegs),
        VMSTATE_UINT32(smi_sts, ICH4MLPCPMRegs),
        VMSTATE_TIMER_PTR_V(periodic_timer, ICH4MLPCPMRegs, 2),
        VMSTATE_UINT8(disable_s3, ICH4MLPCPMRegs),
        VMSTATE_UINT8(disable_s4, ICH4MLPCPMRegs),
        VMSTATE_UINT8(s4_val, ICH4MLPCPMRegs),
        VMSTATE_END_OF_LIST()
    }
};

void ich4m_pm_reset(ICH4MLPCPMRegs *pm)
{
    acpi_pm1_evt_reset(&pm->acpi_regs);
    acpi_pm1_cnt_reset(&pm->acpi_regs);
    acpi_pm_tmr_reset(&pm->acpi_regs);
    acpi_gpe_reset(&pm->acpi_regs);
    pm->smi_en = 0x00000002;
    pm->smi_sts = 0;
    timer_del(pm->periodic_timer);
}

void ich4m_pm_iospace_update(ICH4MLPCPMRegs *pm, uint32_t pm_io_base)
{
    assert((pm_io_base & ICH4M_PMIO_MASK) == 0);

    /* Remove old region if any */
    if (pm->pm_io_base) {
        memory_region_del_subregion(get_system_io(), &pm->io);
    }

    pm->pm_io_base = pm_io_base;
    memory_region_transaction_begin();
    if (pm->pm_io_base != 0) {
        memory_region_add_subregion(get_system_io(), pm->pm_io_base, &pm->io);
        memory_region_set_enabled(&pm->io, true);
    } else {
        memory_region_set_enabled(&pm->io, false);
    }
    memory_region_transaction_commit();
}

void ich4m_pm_init(PCIDevice *lpc_pci, ICH4MLPCPMRegs *pm, qemu_irq sci_irq)
{
    pm->pm_io_base = 0;
    pm->disable_s3 = 0;
    pm->disable_s4 = 0;
    pm->s4_val = 1;

    memory_region_init(&pm->io, OBJECT(lpc_pci), "ich4m-pm", ICH4M_PMIO_SIZE);
    memory_region_set_enabled(&pm->io, false);

    /* GPE (General Purpose Event) registers */
    memory_region_init_io(&pm->io_gpe, OBJECT(lpc_pci), &ich4m_gpe_ops, pm,
                          "ich4m-gpe", ICH4M_PMIO_GPE0_LEN);
    memory_region_add_subregion(&pm->io, ICH4M_PMIO_GPE0_STS, &pm->io_gpe);

    memory_region_init_io(&pm->io_smi, OBJECT(lpc_pci), &ich4m_smi_ops, pm,
                          "ich4m-smi", 8);
    memory_region_add_subregion(&pm->io, ICH4M_PMIO_SMI_EN, &pm->io_smi);

    pm->irq = sci_irq;

    acpi_pm_tmr_init(&pm->acpi_regs, pm_tmr_timer, &pm->io);
    acpi_pm1_evt_init(&pm->acpi_regs, pm_tmr_timer, &pm->io);
    acpi_pm1_cnt_init(&pm->acpi_regs, &pm->io, pm->disable_s3, pm->disable_s4,
                     pm->s4_val, true);
    acpi_gpe_init(&pm->acpi_regs, ICH4M_PMIO_GPE0_LEN);
    pm->periodic_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                      ich4m_pm_periodic_timer_expired, pm);
    ich4m_pm_reset(pm);
}

void ich4m_pm_add_properties(Object *obj, ICH4MLPCPMRegs *pm)
{
    static const uint32_t gpe0_len = ICH4M_PMIO_GPE0_LEN;

    object_property_add_uint32_ptr(obj, ACPI_PM_PROP_PM_IO_BASE,
                                   &pm->pm_io_base, OBJ_PROP_FLAG_READ);
    object_property_add_uint32_ptr(obj, ACPI_PM_PROP_GPE0_BLK,
                                   &pm->pm_io_base, OBJ_PROP_FLAG_READ);
    object_property_add_uint32_ptr(obj, ACPI_PM_PROP_GPE0_BLK_LEN,
                                   &gpe0_len, OBJ_PROP_FLAG_READ);
    object_property_add_uint8_ptr(obj, ACPI_PM_PROP_S3_DISABLED,
                                  &pm->disable_s3, OBJ_PROP_FLAG_READWRITE);
    object_property_add_uint8_ptr(obj, ACPI_PM_PROP_S4_DISABLED,
                                  &pm->disable_s4, OBJ_PROP_FLAG_READWRITE);
    object_property_add_uint8_ptr(obj, ACPI_PM_PROP_S4_VAL,
                                  &pm->s4_val, OBJ_PROP_FLAG_READWRITE);
}
