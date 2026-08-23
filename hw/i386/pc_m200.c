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
    pc_memory_init(pcms, system_memory, pci_memory, 0);

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

    /* Video (NVIDIA GeForce FX Go5200 / NV34M on AGP Bus 1) & Network */
    PCIBus *agp_bus = pci_bridge_get_sec_bus(PCI_BRIDGE(&I855PM_HOST_DEVICE(phb)->agp));
    pci_create_simple(agp_bus ? agp_bus : pcms->pcibus, PCI_DEVFN(0, 0), TYPE_NV34M_VGA);
    pc_nic_init(pcmc, isa_bus, pcms->pcibus);
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


