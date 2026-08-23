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
#include "hw/southbridge/ich9.h"
#include "hw/display/nv34m.h"
#include "qemu/datadir.h"
#include "hw/core/loader.h"

/* Toshiba Portege M200 Motherboard Straps & Hardware ID (Port 0x85) */
static uint8_t toshiba_m200_port85 = 0x00;

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

/* ACPI Embedded Controller (Ports 0x62 Data / 0x66 Status & Command) */
typedef struct ToshibaAcpiECState {
    uint8_t status;
    uint8_t cmd;
    uint8_t data_index;
} ToshibaAcpiECState;

static uint64_t toshiba_acpi_ec_data_read(void *opaque, hwaddr addr, unsigned size)
{
    ToshibaAcpiECState *ec = opaque;
    ec->status &= ~0x01; /* Clear OBF */
    return 0x01; /* EC Status / Power / Security Signature Valid (0x01) */
}

static void toshiba_acpi_ec_data_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    ToshibaAcpiECState *ec = opaque;
    ec->status |= 0x01; /* Set OBF (Output Buffer Full) */
}

static uint64_t toshiba_acpi_ec_cmd_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0x71; /* 0x70 = System Power ON, AC connected, Lid Open | 0x01 = OBF data ready */
}

static void toshiba_acpi_ec_cmd_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    ToshibaAcpiECState *ec = opaque;
    ec->cmd = val & 0xff;
    ec->status |= 0x01; /* Set OBF (Output Buffer Full) */
}

/* Toshiba Portege M200 Hardware Debug Port (PJ2000 - MS Debug Port Spec v1.0 on 0x1E0..0x1E7) */
typedef struct ToshibaDPortState {
    uint8_t byte_count;
    uint16_t current_code;
} ToshibaDPortState;

static const char *dport_code_name(uint16_t code)
{
    switch (code) {
    case 0xF000: return "Early init (A20, PIT CH0, BootBlock Checksum)";
    case 0xF001: return "EC/KBC check";
    case 0xF002: return "Security controller init";
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
    case 0xFFFF: return "Boot complete (Normal status)";
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
            dp->current_code = (val & 0xff);
            dp->byte_count = 1;
        } else {
            dp->current_code |= ((val & 0xff) << 8);
            dp->byte_count = 0;
            fprintf(stderr, "\n>>> [TOSHIBA D-PORT PJ2000] 0x%04X : %s <<<\n",
                    dp->current_code, dport_code_name(dp->current_code));
        }
    }
}

static const MemoryRegionOps toshiba_dport_ops = {
    .read = toshiba_dport_read,
    .write = toshiba_dport_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps toshiba_acpi_ec_data_ops = {
    .read = toshiba_acpi_ec_data_read,
    .write = toshiba_acpi_ec_data_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps toshiba_acpi_ec_cmd_ops = {
    .read = toshiba_acpi_ec_cmd_read,
    .write = toshiba_acpi_ec_cmd_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Intel ICH4-M ACPI Power Management Timer (3.579545 MHz counter on Port 0xD808 / ACPI Base 0xD800) */
static uint32_t toshiba_pm_timer_val = 0;

static uint64_t toshiba_acpi_pm_read(void *opaque, hwaddr addr, unsigned size)
{
    if (addr >= 0x08 && addr < 0x0c) {
        toshiba_pm_timer_val = (toshiba_pm_timer_val + 50000) & 0x00ffffff;
        return toshiba_pm_timer_val;
    }
    return 0;
}

static void toshiba_acpi_pm_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
}

static const MemoryRegionOps toshiba_acpi_pm_ops = {
    .read = toshiba_acpi_pm_read,
    .write = toshiba_acpi_pm_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
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

/* Intel ICH4-M SMBus Controller (Base 0xD880) & DDR SDRAM SPD EEPROM */
typedef struct ToshibaSMBusState {
    uint8_t hst_sts;
    uint8_t hst_cnt;
    uint8_t hst_cmd;
    uint8_t xmit_slva;
    uint8_t hst_d0;
} ToshibaSMBusState;

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
    switch (addr) {
    case 0x00: /* HST_STS */
        val = s->hst_sts;
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
    default:
        val = 0;
        break;
    }
    fprintf(stderr, ">>> [TOSHIBA SMBUS READ] Port 0x%04X -> 0x%02X\n", (unsigned)(0xD880 + addr), val);
    return val;
}

static void toshiba_smbus_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    ToshibaSMBusState *s = opaque;
    fprintf(stderr, ">>> [TOSHIBA SMBUS WRITE] Port 0x%04X <- 0x%02X\n", (unsigned)(0xD880 + addr), (uint8_t)val);
    switch (addr) {
    case 0x00: /* HST_STS (W1C) */
        s->hst_sts &= ~(val & 0xff);
        break;
    case 0x02: /* HST_CNT */
        s->hst_cnt = val & 0xff;
        if (val & 0x40) { /* START */
            uint8_t dev_addr = s->xmit_slva >> 1;
            if (dev_addr == 0x50) { /* DIMM 0 (Slave 0xA0/0xA1) */
                uint8_t offset = s->hst_cmd & 0x7f;
                s->hst_d0 = ddr_spd_dimm0[offset];
                s->hst_sts = 0x02; /* INTR (Transaction Complete) */
                fprintf(stderr, ">>> [TOSHIBA SMBUS] DIMM 0 Read SPD[0x%02X] = 0x%02X\n", offset, s->hst_d0);
            } else {
                s->hst_sts = 0x04; /* DEV_ERR (No device at address) */
                fprintf(stderr, ">>> [TOSHIBA SMBUS] Slave 0x%02X (dev 0x%02X) NOT PRESENT\n", s->xmit_slva, dev_addr);
            }
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
        pcms->smbus = I2C_BUS(qdev_get_child_bus(DEVICE(smb), "i2c"));
        smbus_eeprom_init(pcms->smbus, 8, NULL, 0);
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
    }

    /* Video (NVIDIA GeForce FX Go5200 / NV34M) & Network */
    pci_create_simple(pcms->pcibus, PCI_DEVFN(2, 0), TYPE_NV34M_VGA);
    pc_nic_init(pcmc, isa_bus, pcms->pcibus);

    /* Map NVIDIA GeForce FX Go5200 VBIOS at 0x000C0000 */
    char *vbios_file = qemu_find_file(QEMU_FILE_TYPE_BIOS, "nv34m_vbios.bin");
    if (vbios_file) {
        int vbios_size = get_image_size(vbios_file, NULL);
        if (vbios_size > 0 && vbios_size <= 0x10000) {
            MemoryRegion *vbios_rom = g_new(MemoryRegion, 1);
            memory_region_init_ram(vbios_rom, NULL, "m200.vbios", 0x10000, &error_fatal);
            load_image_size(vbios_file, memory_region_get_ram_ptr(vbios_rom), vbios_size);
            memory_region_add_subregion_overlap(system_memory, 0xc0000, vbios_rom, 1000);
        }
        g_free(vbios_file);
    }

    /* Map 64KB Shadow RAM at 0x000E0000 (preloaded with Module 5, writeable for Option ROM init) */
    MemoryRegion *shadow_ram_e0000 = g_new(MemoryRegion, 1);
    memory_region_init_ram(shadow_ram_e0000, NULL, "m200.shadow-e0000", 0x10000, &error_fatal);
    if (machine->firmware) {
        char *filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, machine->firmware);
        if (filename) {
            FILE *f = fopen(filename, "rb");
            if (f) {
                fseek(f, 0x60000, SEEK_SET); /* Module 5 offset in 512KB ROM */
                if (fread(memory_region_get_ram_ptr(shadow_ram_e0000), 1, 0x10000, f) == 0x10000) {
                    /* Successfully preloaded Module 5 */
                }
                fclose(f);
            }
            g_free(filename);
        }
    }
    memory_region_add_subregion_overlap(system_memory, 0xe0000, shadow_ram_e0000, 1000);

    /* Map 64KB Shadow RAM at 0x000F0000 (preloaded with Module 2 System BIOS, writeable for Stage 0x03) */
    MemoryRegion *shadow_ram_f0000 = g_new(MemoryRegion, 1);
    memory_region_init_ram(shadow_ram_f0000, NULL, "m200.shadow-f0000", 0x10000, &error_fatal);
    if (machine->firmware) {
        char *filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, machine->firmware);
        if (filename) {
            FILE *f = fopen(filename, "rb");
            if (f) {
                fseek(f, 0x20000, SEEK_SET); /* Module 2 (System BIOS) offset in 512KB ROM */
                if (fread(memory_region_get_ram_ptr(shadow_ram_f0000), 1, 0x10000, f) == 0x10000) {
                    /* Successfully preloaded System BIOS */
                }
                /* Place Boot Block reset jump at 0x000FFFF0 so CPU starts in Boot Block */
                uint8_t *ptr = (uint8_t *)memory_region_get_ram_ptr(shadow_ram_f0000);
                ptr[0xfff0] = 0xea;
                ptr[0xfff1] = 0xa0;
                ptr[0xfff2] = 0x3f;
                ptr[0xfff3] = 0x00;
                ptr[0xfff4] = 0xfc;
                fclose(f);
            }
            g_free(filename);
        }
    }
    memory_region_add_subregion_overlap(system_memory, 0xf0000, shadow_ram_f0000, 1000);

    /* Initialize BDA Reset Word at 0x0472 to 0x0000 (Normal Boot) */
    memset(memory_region_get_ram_ptr(machine->ram) + 0x472, 0, 2);

    /* Initialize Interrupt Vector Table (IVT) with default IRET (F000:FF53) and INT 10h (C000:0050) */
    uint32_t *ivt = (uint32_t *)memory_region_get_ram_ptr(machine->ram);
    for (i = 0; i < 256; i++) {
        ivt[i] = 0xf000ff53; /* F000:FF53 (IRET) */
    }
    ivt[0x10] = 0xc0000050; /* C000:0050 (NVIDIA VBIOS INT 10h) */

    /* Place safety IRET at 0x00000000 so any jumps to 0x0000:0000 return cleanly */
    *(uint8_t *)memory_region_get_ram_ptr(machine->ram) = 0xcf;

    /* Initialize BDA Video Variables (0x0040:0x0049..0x0089) for VGA Color Display */
    uint8_t *ram = (uint8_t *)memory_region_get_ram_ptr(machine->ram);
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

    /* ACPI Embedded Controller (Ports 0x62 Data / 0x66 Status & Command) */
    ToshibaAcpiECState *acpi_ec = g_new0(ToshibaAcpiECState, 1);
    MemoryRegion *acpi_ec_data = g_new(MemoryRegion, 1);
    MemoryRegion *acpi_ec_cmd = g_new(MemoryRegion, 1);
    memory_region_init_io(acpi_ec_data, OBJECT(machine), &toshiba_acpi_ec_data_ops, acpi_ec, "acpi-ec-data", 1);
    memory_region_init_io(acpi_ec_cmd, OBJECT(machine), &toshiba_acpi_ec_cmd_ops, acpi_ec, "acpi-ec-cmd", 1);
    memory_region_add_subregion(system_io, 0x62, acpi_ec_data);
    memory_region_add_subregion(system_io, 0x66, acpi_ec_cmd);

    /* Toshiba Hardware Debug Port (PJ2000 on Ports 0x1E0..0x1E7) */
    ToshibaDPortState *dport = g_new0(ToshibaDPortState, 1);
    MemoryRegion *dport_io = g_new(MemoryRegion, 1);
    memory_region_init_io(dport_io, OBJECT(machine), &toshiba_dport_ops, dport, "toshiba-dport", 8);
    memory_region_add_subregion(system_io, 0x1e0, dport_io);

    /* Intel ICH4-M ACPI Power Management Timer (Base 0xD800, Timer at 0xD808) */
    MemoryRegion *acpi_pm = g_new(MemoryRegion, 1);
    memory_region_init_io(acpi_pm, OBJECT(machine), &toshiba_acpi_pm_ops, NULL, "m200-acpi-pm", 0x40);
    memory_region_add_subregion_overlap(system_io, 0xd800, acpi_pm, 100);

    /* Port 0x64 KBC Status (SYSF set) */
    MemoryRegion *kbc_status_io = g_new(MemoryRegion, 1);
    memory_region_init_io(kbc_status_io, OBJECT(machine), &toshiba_kbc_status_ops, NULL, "m200-kbc-status", 1);
    memory_region_add_subregion_overlap(system_io, 0x64, kbc_status_io, 50);

    /* Intel ICH4-M SMBus Controller (Base 0xD880) */
    ToshibaSMBusState *smbus = g_new0(ToshibaSMBusState, 1);
    MemoryRegion *smbus_io = g_new(MemoryRegion, 1);
    memory_region_init_io(smbus_io, OBJECT(machine), &toshiba_smbus_ops, smbus, "m200-smbus", 0x20);
    memory_region_add_subregion_overlap(system_io, 0xd880, smbus_io, 100);

    /* Toshiba NoteBIOS High SMRAM / TSEG Window (0xFED00000..0xFEDFFFFF, size 1MB) */
    MemoryRegion *smram_tseg = g_new(MemoryRegion, 1);
    memory_region_init_ram(smram_tseg, NULL, "m200-smram-tseg", 0x100000, &error_fatal);
    memory_region_add_subregion(system_memory, 0xfed00000, smram_tseg);
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
    m->no_floppy = 1;
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


