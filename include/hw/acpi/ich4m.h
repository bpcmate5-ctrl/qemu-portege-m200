/*
 * QEMU ICH4M LPC PM Emulation
 *
 * Copyright (c) 2024
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef HW_ACPI_ICH4M_H
#define HW_ACPI_ICH4M_H

#include "hw/acpi/acpi.h"

#define ACPI_PCIHP_ADDR_ICH4M 0xae00

typedef struct ICH4MLPCPMRegs {
    /*
     * ICH4M uses 16-bit PM1 registers (similar to PIIX4)
     */
    ACPIREGS acpi_regs;

    MemoryRegion io;
    MemoryRegion io_gpe;

    qemu_irq irq;      /* SCI */

    uint32_t pm_io_base;
    Notifier powerdown_notifier;

    uint8_t disable_s3;
    uint8_t disable_s4;
    uint8_t s4_val;
} ICH4MLPCPMRegs;

void ich4m_pm_init(PCIDevice *lpc_pci, ICH4MLPCPMRegs *pm, qemu_irq sci_irq);
void ich4m_pm_add_properties(Object *obj, ICH4MLPCPMRegs *pm);
void ich4m_pm_iospace_update(ICH4MLPCPMRegs *pm, uint32_t pm_io_base);
extern const VMStateDescription vmstate_ich4m_pm;

#endif /* HW_ACPI_ICH4M_H */

