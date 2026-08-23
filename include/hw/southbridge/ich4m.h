/*
 * QEMU ICH4M South Bridge Emulation
 *
 * Copyright (c) 2024
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef HW_SOUTHBRIDGE_ICH4M_H
#define HW_SOUTHBRIDGE_ICH4M_H

#include "hw/isa/apm.h"
#include "hw/intc/ioapic.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_device.h"
#include "hw/rtc/mc146818rtc.h"
#include "hw/acpi/ich4m.h"
#include "system/memory.h"
#include "qom/object.h"

#define TYPE_ICH4M_LPC_DEVICE "ICH4M-LPC"
OBJECT_DECLARE_SIMPLE_TYPE(ICH4MLPCState, ICH4M_LPC_DEVICE)

#define ICH4M_GPIO_GSI "gsi"

struct ICH4MLPCState {
    /* ICH4M LPC PCI to ISA bridge */
    PCIDevice d;

    /* Interrupt routing: (pci device, intx) -> pirq */
    uint8_t irr[PCI_SLOT_MAX][PCI_NUM_PINS];

    MC146818RtcState rtc;
    APMState apm;
    ICH4MLPCPMRegs pm;

    uint32_t sci_level; /* track sci level */
    uint8_t sci_gsi;

    /* Reset Control Register */
    uint8_t rst_cnt;
    MemoryRegion rst_cnt_mem;

    qemu_irq gsi[IOAPIC_NUM_PINS];
};

/* ICH4M LPC Device at D31:F0 */
#define ICH4M_LPC_DEV                    31
#define ICH4M_LPC_FUNC                   0

#define ICH4M_LPC_PMBASE                 0x40
#define ICH4M_LPC_PMBASE_BASE_ADDRESS_MASK   0xff80
#define ICH4M_LPC_PMBASE_RTE             0x1
#define ICH4M_LPC_PMBASE_DEFAULT         0x1

#define ICH4M_LPC_ACPI_CTRL              0x44
#define ICH4M_LPC_ACPI_CTRL_ACPI_EN      0x10  /* Bit 4 on ICH4-M */
#define ICH4M_LPC_ACPI_CTRL_SCI_IRQ_SEL_MASK  0x07
#define ICH4M_LPC_ACPI_CTRL_9            0x0
#define ICH4M_LPC_ACPI_CTRL_10           0x1
#define ICH4M_LPC_ACPI_CTRL_11           0x2
#define ICH4M_LPC_ACPI_CTRL_20           0x4
#define ICH4M_LPC_ACPI_CTRL_DEFAULT      0x0

#define ICH4M_LPC_GPIOBASE               0x58
#define ICH4M_LPC_GPIOBASE_DEFAULT       0x1

/* PIRQ Routing - ICH4M has 8 PIRQs (A-D at 0x60-0x63, E-H at 0x68-0x6B) */
#define ICH4M_LPC_PIRQA_ROUT             0x60
#define ICH4M_LPC_PIRQB_ROUT             0x61
#define ICH4M_LPC_PIRQC_ROUT             0x62
#define ICH4M_LPC_PIRQD_ROUT             0x63
#define ICH4M_LPC_PIRQE_ROUT             0x68
#define ICH4M_LPC_PIRQF_ROUT             0x69
#define ICH4M_LPC_PIRQG_ROUT             0x6a
#define ICH4M_LPC_PIRQH_ROUT             0x6b

#define ICH4M_LPC_PIRQ_ROUT_IRQEN        0x80  /* 0x80 = Route Disabled */
#define ICH4M_LPC_PIRQ_ROUT_MASK         0x0f
#define ICH4M_LPC_PIRQ_ROUT_DEFAULT      0x80

#define ICH4M_LPC_NB_PIRQS               8       /* PIRQ[A-H] */

/* D31:F0 Reset Control Register */
#define ICH4M_RST_CNT_IOPORT             0xcf9

/* D31:F1 IDE Controller */
#define ICH4M_IDE_DEV                    31
#define ICH4M_IDE_FUNC                   1

/* D31:F3 SMBus Controller */
#define ICH4M_SMB_DEV                    31
#define ICH4M_SMB_FUNC                   3

/* D31 USB UHCI Controllers - ICH4M has 3 UHCI (F2, F4, F5) */
#define ICH4M_USB_UHCI1_DEV              31
#define ICH4M_USB_UHCI1_FUNC             2

#define ICH4M_USB_UHCI2_DEV              31
#define ICH4M_USB_UHCI2_FUNC             4

#define ICH4M_USB_UHCI3_DEV              31
#define ICH4M_USB_UHCI3_FUNC             5

/* D31:F6 USB EHCI Controller */
#define ICH4M_USB_EHCI_DEV               31
#define ICH4M_USB_EHCI_FUNC              6

/* D31:F5 AC97 Audio */
#define ICH4M_AC97_DEV                   31
#define ICH4M_AC97_FUNC                  5

/* ICH4M Power Management I/O registers offset from PMBASE */
#define ICH4M_PMIO_SIZE                  128
#define ICH4M_PMIO_MASK                  (ICH4M_PMIO_SIZE - 1)

#define ICH4M_PMIO_PM1_STS               0x00
#define ICH4M_PMIO_PM1_EN                0x02
#define ICH4M_PMIO_PM1_CNT               0x04
#define ICH4M_PMIO_PM1_TMR               0x08
#define ICH4M_PMIO_GPE0_STS              0x20
#define ICH4M_PMIO_GPE0_EN               0x28
#define ICH4M_PMIO_GPE0_LEN              4

/* FADT ACPI_ENABLE/ACPI_DISABLE */
#define ICH4M_APM_ACPI_ENABLE            0x2
#define ICH4M_APM_ACPI_DISABLE           0x3

#endif /* HW_SOUTHBRIDGE_ICH4M_H */
