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
    .version_id = 1,
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
        VMSTATE_UINT8(disable_s3, ICH4MLPCPMRegs),
        VMSTATE_UINT8(disable_s4, ICH4MLPCPMRegs),
        VMSTATE_UINT8(s4_val, ICH4MLPCPMRegs),
        VMSTATE_END_OF_LIST()
    }
};

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

    pm->irq = sci_irq;

    acpi_pm_tmr_init(&pm->acpi_regs, pm_tmr_timer, &pm->io);
    acpi_pm1_evt_init(&pm->acpi_regs, pm_tmr_timer, &pm->io);
    acpi_pm1_cnt_init(&pm->acpi_regs, &pm->io, pm->disable_s3, pm->disable_s4,
                     pm->s4_val, true);
    acpi_gpe_init(&pm->acpi_regs, ICH4M_PMIO_GPE0_LEN);
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

