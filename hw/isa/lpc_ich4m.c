/*
 * QEMU ICH4M LPC Bridge Emulation
 *
 * Copyright (c) 2024
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "qapi/error.h"
#include "qemu/range.h"
#include "hw/dma/i8257.h"
#include "hw/isa/isa.h"
#include "migration/vmstate.h"
#include "hw/core/irq.h"
#include "hw/isa/apm.h"
#include "hw/pci/pci.h"
#include "hw/southbridge/ich4m.h"
#include "hw/acpi/acpi.h"
#include "hw/acpi/ich4m.h"
#include "hw/pci/pci_bus.h"
#include "hw/core/qdev-properties.h"
#include "system/runstate.h"
#include "system/system.h"
#include "hw/core/cpu.h"
#include "hw/nvram/fw_cfg.h"
#include "qemu/cutils.h"
#include "hw/rtc/mc146818rtc.h"
#include "hw/intc/i8259.h"
#include "hw/timer/i8254.h"

/*****************************************************************************/
/* ICH4M LPC PCI to ISA bridge */

static void ich4m_lpc_rout(uint8_t pirq_rout, int *pic_irq, int *pic_dis)
{
    *pic_irq = pirq_rout & ICH4M_LPC_PIRQ_ROUT_MASK;
    *pic_dis = pirq_rout & ICH4M_LPC_PIRQ_ROUT_IRQEN;
}

static void ich4m_lpc_pic_irq(ICH4MLPCState *lpc, int pirq_num,
                             int *pic_irq, int *pic_dis)
{
    switch (pirq_num) {
    case 0 ... 3: /* A-D */
        ich4m_lpc_rout(lpc->d.config[ICH4M_LPC_PIRQA_ROUT + pirq_num],
                      pic_irq, pic_dis);
        return;
    case 4 ... 7: /* E-H */
        ich4m_lpc_rout(lpc->d.config[ICH4M_LPC_PIRQE_ROUT + (pirq_num - 4)],
                      pic_irq, pic_dis);
        return;
    default:
        break;
    }
    *pic_irq = -1;
    *pic_dis = 1;
}

static int ich4m_pirq_to_gsi(int pirq)
{
    return pirq + 16;
}

static int ich4m_gsi_to_pirq(int gsi)
{
    return gsi - 16;
}

static void ich4m_lpc_update_apic(ICH4MLPCState *lpc, int gsi)
{
    int level = 0;

    if (gsi < 16) {
        return;
    }

    level |= pci_bus_get_irq_level(pci_get_bus(&lpc->d), ich4m_gsi_to_pirq(gsi));
    if (gsi == lpc->sci_gsi) {
        level |= lpc->sci_level;
    }

    qemu_set_irq(lpc->gsi[gsi], level);
}

static void ich4m_lpc_update_pic(ICH4MLPCState *lpc, int gsi)
{
    int i, pic_level = 0;

    if (gsi >= 16) {
        return;
    }

    for (i = 0; i < ICH4M_LPC_NB_PIRQS; i++) {
        int tmp_irq, tmp_dis;
        ich4m_lpc_pic_irq(lpc, i, &tmp_irq, &tmp_dis);
        if (!tmp_dis && tmp_irq == gsi) {
            pic_level |= pci_bus_get_irq_level(pci_get_bus(&lpc->d), i);
        }
    }
    if (gsi == lpc->sci_gsi) {
        pic_level |= lpc->sci_level;
    }

    qemu_set_irq(lpc->gsi[gsi], pic_level);
}

static void ich4m_set_pci_irq(void *opaque, int pirq, int level)
{
    ICH4MLPCState *lpc = opaque;
    int pic_irq, pic_dis;

    assert(0 <= pirq && pirq < ICH4M_LPC_NB_PIRQS);

    ich4m_lpc_update_apic(lpc, ich4m_pirq_to_gsi(pirq));
    ich4m_lpc_pic_irq(lpc, pirq, &pic_irq, &pic_dis);
    ich4m_lpc_update_pic(lpc, pic_irq);
}

static PCIINTxRoute ich4m_route_intx_pin_to_irq(void *opaque, int pirq_pin)
{
    ICH4MLPCState *lpc = opaque;
    PCIINTxRoute route;
    int pic_irq, pic_dis;

    assert(0 <= pirq_pin && pirq_pin < ICH4M_LPC_NB_PIRQS);

    route.mode = PCI_INTX_ENABLED;
    ich4m_lpc_pic_irq(lpc, pirq_pin, &pic_irq, &pic_dis);
    if (!pic_dis) {
        if (pic_irq < 16) {
            route.irq = pic_irq;
        } else {
            route.mode = PCI_INTX_DISABLED;
            route.irq = -1;
        }
    } else {
        route.irq = ich4m_pirq_to_gsi(pirq_pin);
    }
    return route;
}

static int ich4m_lpc_map_irq(PCIDevice *pci_dev, int intx)
{
    BusState *bus = qdev_get_parent_bus(&pci_dev->qdev);
    PCIBus *pci_bus = PCI_BUS(bus);
    PCIDevice *lpc_pdev =
            pci_bus->devices[PCI_DEVFN(ICH4M_LPC_DEV, ICH4M_LPC_FUNC)];
    ICH4MLPCState *lpc = ICH4M_LPC_DEVICE(lpc_pdev);

    return lpc->irr[PCI_SLOT(pci_dev->devfn)][intx];
}

static void ich4m_apm_ctrl_changed(uint32_t val, void *arg)
{
    ICH4MLPCState *lpc = arg;

    /* ACPI specs 3.0, 4.7.2.5 */
    acpi_pm1_cnt_update(&lpc->pm.acpi_regs,
                        val == ICH4M_APM_ACPI_ENABLE,
                        val == ICH4M_APM_ACPI_DISABLE);
    if (val == ICH4M_APM_ACPI_ENABLE || val == ICH4M_APM_ACPI_DISABLE) {
        return;
    }
}

/* config:PMBASE */
static void ich4m_lpc_pmbase_sci_update(ICH4MLPCState *lpc)
{
    uint32_t pm_io_base = pci_get_long(lpc->d.config + ICH4M_LPC_PMBASE);
    uint8_t acpi_cntl = lpc->d.config[ICH4M_LPC_ACPI_CTRL];
    int new_gsi;

    if (acpi_cntl & ICH4M_LPC_ACPI_CTRL_ACPI_EN) {
        pm_io_base &= ICH4M_LPC_PMBASE_BASE_ADDRESS_MASK;
    } else {
        pm_io_base = 0;
    }

    ich4m_pm_iospace_update(&lpc->pm, pm_io_base);

    new_gsi = (acpi_cntl & ICH4M_LPC_ACPI_CTRL_SCI_IRQ_SEL_MASK);
    if (new_gsi == 0) {
        return; /* Disabled */
    }
    if (lpc->sci_level && new_gsi != lpc->sci_gsi) {
        qemu_set_irq(lpc->pm.irq, 0);
        lpc->sci_gsi = new_gsi;
        qemu_set_irq(lpc->pm.irq, 1);
    }
    lpc->sci_gsi = new_gsi;
}

static void ich4m_lpc_config_write(PCIDevice *d,
                                   uint32_t addr, uint32_t val, int len)
{
    ICH4MLPCState *lpc = ICH4M_LPC_DEVICE(d);

    pci_default_write_config(d, addr, val, len);
    if (ranges_overlap(addr, len, ICH4M_LPC_PMBASE, 4) ||
        ranges_overlap(addr, len, ICH4M_LPC_ACPI_CTRL, 1)) {
        ich4m_lpc_pmbase_sci_update(lpc);
    }
    if (ranges_overlap(addr, len, ICH4M_LPC_PIRQA_ROUT, 4) ||
        ranges_overlap(addr, len, ICH4M_LPC_PIRQE_ROUT, 4)) {
        pci_bus_fire_intx_routing_notifier(pci_get_bus(&lpc->d));
    }
}

static void ich4m_lpc_reset(DeviceState *qdev)
{
    PCIDevice *d = PCI_DEVICE(qdev);
    ICH4MLPCState *lpc = ICH4M_LPC_DEVICE(d);
    int i;

    /* Initialize PIRQ routing matching Toshiba Portégé M200 dump */
    for (i = 0; i < 4; i++) {
        pci_set_byte(d->config + ICH4M_LPC_PIRQA_ROUT + i, 0x0b); /* IRQ 11 */
    }
    pci_set_byte(d->config + ICH4M_LPC_PIRQE_ROUT, 0x0b);
    pci_set_byte(d->config + ICH4M_LPC_PIRQF_ROUT, 0x80); /* disabled */
    pci_set_byte(d->config + ICH4M_LPC_PIRQG_ROUT, 0x0a); /* IRQ 10 */
    pci_set_byte(d->config + ICH4M_LPC_PIRQH_ROUT, 0x0b); /* IRQ 11 */

    /* Reset PMBASE */
    pci_set_long(d->config + ICH4M_LPC_PMBASE, ICH4M_LPC_PMBASE_DEFAULT);
    pci_set_byte(d->config + ICH4M_LPC_ACPI_CTRL, ICH4M_LPC_ACPI_CTRL_DEFAULT);
    pci_set_long(d->config + ICH4M_LPC_GPIOBASE, ICH4M_LPC_GPIOBASE_DEFAULT);

    /* Reset Control Register */
    lpc->rst_cnt = 0;

    ich4m_lpc_pmbase_sci_update(lpc);
}

static void ich4m_rst_cnt_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned width)
{
    ICH4MLPCState *lpc = opaque;

    fprintf(stderr, "[PORT CF9 W] val=0x%02x\n", (uint8_t)val);

    if (val & 4) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        return;
    }
    lpc->rst_cnt = val & 0xA; /* keep FULL_RST (bit 3) and SYS_RST (bit 1) */
}

static uint64_t ich4m_rst_cnt_read(void *opaque, hwaddr addr, unsigned width)
{
    ICH4MLPCState *lpc = opaque;
    return lpc->rst_cnt;
}

static const MemoryRegionOps ich4m_rst_cnt_ops = {
    .read = ich4m_rst_cnt_read,
    .write = ich4m_rst_cnt_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 1,
};

static int ich4m_lpc_post_load(void *opaque, int version_id)
{
    ICH4MLPCState *lpc = opaque;

    ich4m_lpc_pmbase_sci_update(lpc);
    return 0;
}

static const VMStateDescription vmstate_ich4m_lpc = {
    .name = "ICH4MLPC",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = ich4m_lpc_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(d, ICH4MLPCState),
        VMSTATE_STRUCT(apm, ICH4MLPCState, 0, vmstate_apm, APMState),
        VMSTATE_STRUCT(pm, ICH4MLPCState, 0, vmstate_ich4m_pm, ICH4MLPCPMRegs),
        VMSTATE_UINT32(sci_level, ICH4MLPCState),
        VMSTATE_UINT8(rst_cnt, ICH4MLPCState),
        VMSTATE_END_OF_LIST()
    }
};

static void ich4m_set_sci(void *opaque, int irq_num, int level)
{
    ICH4MLPCState *lpc = opaque;

    lpc->sci_level = level;
    ich4m_lpc_update_apic(lpc, lpc->sci_gsi);
    ich4m_lpc_update_pic(lpc, lpc->sci_gsi);
}

static void ich4m_lpc_initfn(Object *obj)
{
    ICH4MLPCState *lpc = ICH4M_LPC_DEVICE(obj);
    static const uint8_t acpi_enable_cmd = ICH4M_APM_ACPI_ENABLE;
    static const uint8_t acpi_disable_cmd = ICH4M_APM_ACPI_DISABLE;

    object_initialize_child(obj, "rtc", &lpc->rtc, TYPE_MC146818_RTC);
    qdev_init_gpio_out_named(DEVICE(lpc), lpc->gsi, ICH4M_GPIO_GSI, IOAPIC_NUM_PINS);

    object_property_add_uint8_ptr(obj, ACPI_PM_PROP_SCI_INT,
                                  &lpc->sci_gsi, OBJ_PROP_FLAG_READ);
    object_property_add_uint8_ptr(OBJECT(lpc), ACPI_PM_PROP_ACPI_ENABLE_CMD,
                                  &acpi_enable_cmd, OBJ_PROP_FLAG_READ);
    object_property_add_uint8_ptr(OBJECT(lpc), ACPI_PM_PROP_ACPI_DISABLE_CMD,
                                  &acpi_disable_cmd, OBJ_PROP_FLAG_READ);

    ich4m_pm_add_properties(obj, &lpc->pm);
}

static void ich4m_lpc_realize(PCIDevice *d, Error **errp)
{
    ICH4MLPCState *lpc = ICH4M_LPC_DEVICE(d);
    PCIBus *pci_bus = pci_get_bus(d);
    ISABus *isa_bus;
    uint32_t irq;
    int i, j;

    isa_bus = isa_bus_new(DEVICE(d), get_system_memory(), get_system_io(),
                          errp);
    if (!isa_bus) {
        return;
    }

    /* Set Toshiba Subsystem Vendor & Subsystem Device IDs */
    pci_set_word(d->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
    pci_set_word(d->config + PCI_SUBSYSTEM_ID, 0x0001);

    /* Setup PMBASE and ACPI write masks */
    pci_set_long(d->wmask + ICH4M_LPC_PMBASE,
                 ICH4M_LPC_PMBASE_BASE_ADDRESS_MASK);
    pci_set_byte(d->wmask + ICH4M_LPC_ACPI_CTRL, 0xff);
    pci_set_long(d->wmask + ICH4M_LPC_GPIOBASE, 0xffc0);

    /* Setup PIRQ write masks */
    memset(d->wmask + ICH4M_LPC_PIRQA_ROUT, 0xff, 4);
    memset(d->wmask + ICH4M_LPC_PIRQE_ROUT, 0xff, 4);

    /* Initialize APM */
    apm_init(d, &lpc->apm, ich4m_apm_ctrl_changed, lpc);

    /* Initialize Power Management (ACPI) */
    qemu_irq sci_irq = qemu_allocate_irq(ich4m_set_sci, lpc, 0);
    ich4m_pm_init(d, &lpc->pm, sci_irq);

    /* Reset Control Register */
    memory_region_init_io(&lpc->rst_cnt_mem, OBJECT(d), &ich4m_rst_cnt_ops, lpc,
                          "lpc-reset-control", 1);
    memory_region_add_subregion_overlap(pci_address_space_io(d),
                                        ICH4M_RST_CNT_IOPORT, &lpc->rst_cnt_mem,
                                        1);

    isa_bus_register_input_irqs(isa_bus, lpc->gsi);

    /* DMA Controller */
    i8257_dma_init(OBJECT(d), isa_bus, 0);

    /* RTC */
    qdev_prop_set_int32(DEVICE(&lpc->rtc), "base_year", 2000);
    if (!qdev_realize(DEVICE(&lpc->rtc), BUS(isa_bus), errp)) {
        return;
    }
    irq = object_property_get_uint(OBJECT(&lpc->rtc), "irq", &error_fatal);
    isa_connect_gpio_out(ISA_DEVICE(&lpc->rtc), 0, irq);

    /* Default PCI interrupt routing matrix */
    for (i = 0; i < PCI_SLOT_MAX; i++) {
        for (j = 0; j < PCI_NUM_PINS; j++) {
            lpc->irr[i][j] = (i + j) % ICH4M_LPC_NB_PIRQS;
        }
    }

    /* PCI Interrupt Routing */
    pci_bus_irqs(pci_bus, ich4m_set_pci_irq, d, ICH4M_LPC_NB_PIRQS);
    pci_bus_map_irqs(pci_bus, ich4m_lpc_map_irq);
    pci_bus_set_route_irq_fn(pci_bus, ich4m_route_intx_pin_to_irq);

    ich4m_lpc_pmbase_sci_update(lpc);
}

static void ich4m_lpc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = ich4m_lpc_realize;
    k->config_write = ich4m_lpc_config_write;
    k->vendor_id = PCI_VENDOR_ID_INTEL;
    k->device_id = PCI_DEVICE_ID_INTEL_82801DBM_LPC;
    k->revision = 0x03; /* ICH4M revision */
    k->class_id = PCI_CLASS_BRIDGE_ISA;
    device_class_set_legacy_reset(dc, ich4m_lpc_reset);
    dc->vmsd = &vmstate_ich4m_lpc;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->desc = "ICH4M LPC Bridge";
    dc->user_creatable = false;
}

static const TypeInfo ich4m_lpc_info = {
    .name          = TYPE_ICH4M_LPC_DEVICE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(ICH4MLPCState),
    .instance_init = ich4m_lpc_initfn,
    .class_init    = ich4m_lpc_class_init,
    .interfaces = (const InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { }
    }
};

static void ich4m_lpc_register_types(void)
{
    type_register_static(&ich4m_lpc_info);
}

type_init(ich4m_lpc_register_types);

