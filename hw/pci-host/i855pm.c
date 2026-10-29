/*
 * QEMU i855PM Memory Controller Hub Emulation
 *
 * Copyright (c) 2024
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/i386/pc.h"
#include "hw/pci-host/i855pm.h"
#include "hw/pci-host/pam.h"
#include "hw/pci/pci_bridge.h"
#include "hw/display/nv34m.h"
#include "hw/intc/ioapic.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/module.h"

/****************************************************************************
 * i855PM host
 */

static void i855pm_host_realize(DeviceState *dev, Error **errp)
{
    PCIHostState *pci = PCI_HOST_BRIDGE(dev);
    I855PMHost *s = I855PM_HOST_DEVICE(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    fprintf(stderr, "[I855PM REALIZE] s->mch.address_space_io = %p (system_io = %p)\n",
            s->mch.address_space_io, get_system_io());

    memory_region_add_subregion(s->mch.address_space_io,
                                I855PM_HOST_BRIDGE_CONFIG_ADDR, &pci->conf_mem);
    sysbus_init_ioports(sbd, I855PM_HOST_BRIDGE_CONFIG_ADDR, 4);

    memory_region_add_subregion(s->mch.address_space_io,
                                I855PM_HOST_BRIDGE_CONFIG_DATA, &pci->data_mem);
    sysbus_init_ioports(sbd, I855PM_HOST_BRIDGE_CONFIG_DATA, 4);

    /* register i855pm 0xcf8 port as coalesced pio */
    memory_region_set_flush_coalesced(&pci->data_mem);
    memory_region_add_coalescing(&pci->conf_mem, 0, 4);

    /* i855PM uses standard PCI bus, not PCIe */
    pci->bus = pci_root_bus_new(DEVICE(s), "pci.0",
                                s->mch.pci_address_space,
                                s->mch.address_space_io,
                                0, TYPE_PCI_BUS);

    qdev_realize(DEVICE(&s->mch), BUS(pci->bus), &error_fatal);
    qdev_realize(DEVICE(&s->agp), BUS(pci->bus), &error_fatal);
}

static const char *i855pm_host_root_bus_path(PCIHostState *host_bridge,
                                             PCIBus *rootbus)
{
    return "0000:00";
}

static void i855pm_host_get_pci_hole_start(Object *obj, Visitor *v,
                                           const char *name, void *opaque,
                                           Error **errp)
{
    I855PMHost *s = I855PM_HOST_DEVICE(obj);
    uint64_t val64;
    uint32_t value;

    val64 = range_is_empty(&s->mch.pci_hole)
        ? 0x80000000ULL : range_lob(&s->mch.pci_hole);
    value = val64;
    assert(value == val64);
    visit_type_uint32(v, name, &value, errp);
}

static void i855pm_host_get_pci_hole_end(Object *obj, Visitor *v,
                                         const char *name, void *opaque,
                                         Error **errp)
{
    I855PMHost *s = I855PM_HOST_DEVICE(obj);
    uint64_t val64;
    uint32_t value;

    val64 = range_is_empty(&s->mch.pci_hole)
        ? 0 : range_upb(&s->mch.pci_hole) + 1;
    value = val64;
    assert(value == val64);
    visit_type_uint32(v, name, &value, errp);
}

static const Property i855pm_host_props[] = {
    DEFINE_PROP_SIZE(PCI_HOST_BELOW_4G_MEM_SIZE, I855PMHost,
                     mch.below_4g_mem_size, 0),
    DEFINE_PROP_SIZE(PCI_HOST_ABOVE_4G_MEM_SIZE, I855PMHost,
                     mch.above_4g_mem_size, 0),
};

static void i855pm_host_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIHostBridgeClass *hc = PCI_HOST_BRIDGE_CLASS(klass);

    hc->root_bus_path = i855pm_host_root_bus_path;
    dc->realize = i855pm_host_realize;
    device_class_set_props(dc, i855pm_host_props);
    /* Reason: needs to be wired up by pc_m200_init */
    dc->user_creatable = false;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->fw_name = "pci";
}

static void i855pm_host_initfn(Object *obj)
{
    I855PMHost *s = I855PM_HOST_DEVICE(obj);
    PCIHostState *phb = PCI_HOST_BRIDGE(obj);

    memory_region_init_io(&phb->conf_mem, obj, &pci_host_conf_le_ops, phb,
                          "pci-conf-idx", 4);
    memory_region_init_io(&phb->data_mem, obj, &pci_host_data_le_ops, phb,
                          "pci-conf-data", 4);

    object_initialize_child(OBJECT(s), "mch", &s->mch, TYPE_I855PM_MCH_DEVICE);
    qdev_prop_set_int32(DEVICE(&s->mch), "addr", PCI_DEVFN(0, 0));
    qdev_prop_set_bit(DEVICE(&s->mch), "multifunction", false);

    object_initialize_child(OBJECT(s), "agp", &s->agp, TYPE_I855PM_AGP_DEVICE);
    qdev_prop_set_int32(DEVICE(&s->agp), "addr", PCI_DEVFN(1, 0));
    qdev_prop_set_bit(DEVICE(&s->agp), "multifunction", false);

    object_property_add(obj, PCI_HOST_PROP_PCI_HOLE_START, "uint32",
                        i855pm_host_get_pci_hole_start,
                        NULL, NULL, NULL);

    object_property_add(obj, PCI_HOST_PROP_PCI_HOLE_END, "uint32",
                        i855pm_host_get_pci_hole_end,
                        NULL, NULL, NULL);

    object_property_add_link(obj, PCI_HOST_PROP_RAM_MEM, TYPE_MEMORY_REGION,
                             (Object **) &s->mch.ram_memory,
                             qdev_prop_allow_set_link_before_realize, 0);

    object_property_add_link(obj, PCI_HOST_PROP_PCI_MEM, TYPE_MEMORY_REGION,
                             (Object **) &s->mch.pci_address_space,
                             qdev_prop_allow_set_link_before_realize, 0);

    object_property_add_link(obj, PCI_HOST_PROP_SYSTEM_MEM, TYPE_MEMORY_REGION,
                             (Object **) &s->mch.system_memory,
                             qdev_prop_allow_set_link_before_realize, 0);

    object_property_add_link(obj, PCI_HOST_PROP_IO_MEM, TYPE_MEMORY_REGION,
                             (Object **) &s->mch.address_space_io,
                             qdev_prop_allow_set_link_before_realize, 0);
}

static const TypeInfo i855pm_host_info = {
    .name       = TYPE_I855PM_HOST_DEVICE,
    .parent     = TYPE_PCI_HOST_BRIDGE,
    .instance_size = sizeof(I855PMHost),
    .instance_init = i855pm_host_initfn,
    .class_init = i855pm_host_class_init,
};

/****************************************************************************
 * i855PM AGP Bridge D1:F0
 */

static void i855pm_agp_realize(PCIDevice *d, Error **errp)
{
    pci_bridge_initfn(d, TYPE_PCI_BUS);
    d->config[PCI_PRIMARY_BUS] = 0x00;
    d->config[PCI_SECONDARY_BUS] = 0x00;
    d->config[PCI_SUBORDINATE_BUS] = 0x00;
    d->config[PCI_BRIDGE_CONTROL] |= PCI_BRIDGE_CTL_VGA;
}

static void i855pm_agp_write_config(PCIDevice *d, uint32_t addr, uint32_t val, int len)
{
    uint8_t old_sec = d->config[PCI_SECONDARY_BUS];
    pci_bridge_write_config(d, addr, val, len);
    uint8_t new_sec = d->config[PCI_SECONDARY_BUS];

    if (addr >= 0x18 && addr <= 0x1b) {
        fprintf(stderr, "[M200 AGP BRIDGE CFG W] 00:01.0 Bus numbers: Primary=%u, Secondary=%u (was %u), Subordinate=%u (eip=0x%08x)\n",
                d->config[PCI_PRIMARY_BUS], new_sec, old_sec, d->config[PCI_SUBORDINATE_BUS], pc_m200_get_cpu_eip());
    } else if (addr == 0x04 || addr == 0x3e) {
        fprintf(stderr, "[M200 AGP BRIDGE CFG W] 00:01.0 CMD=0x%04x, BridgeCtl=0x%04x (eip=0x%08x)\n",
                pci_get_word(d->config + PCI_COMMAND), pci_get_word(d->config + PCI_BRIDGE_CONTROL), pc_m200_get_cpu_eip());
    }
}

static void i855pm_agp_class_init(ObjectClass *klass, const void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_INTEL;
    k->device_id = PCI_DEVICE_ID_INTEL_82855PM_AGP;
    k->revision = I855PM_AGP_BRIDGE_REVISION_DEFAULT;
    k->class_id = PCI_CLASS_BRIDGE_PCI;
    k->realize = i855pm_agp_realize;
    k->config_write = i855pm_agp_write_config;
    dc->vmsd = &vmstate_pci_device;
    device_class_set_legacy_reset(dc, pci_bridge_reset);
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->desc = "i855PM AGP Bridge";
    dc->user_creatable = false;
}

static const TypeInfo i855pm_agp_info = {
    .name          = TYPE_I855PM_AGP_DEVICE,
    .parent        = TYPE_PCI_BRIDGE,
    .instance_size = sizeof(I855PMAGPState),
    .class_init    = i855pm_agp_class_init,
    .interfaces = (const InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

/****************************************************************************
 * i855PM MCH D0:F0
 */

static uint64_t i855pm_blackhole_read(void *opaque, hwaddr addr,
                                      unsigned size)
{
    return UINT64_MAX;
}

static void i855pm_blackhole_write(void *opaque, hwaddr addr, uint64_t value,
                                   unsigned size)
{
}

static const MemoryRegionOps i855pm_blackhole_ops = {
    .read = i855pm_blackhole_read,
    .write = i855pm_blackhole_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};

/* PAM */
static void i855pm_mch_update_pam(I855PMMCHState *mch)
{
    PCIDevice *pd = PCI_DEVICE(mch);
    int i;

    memory_region_transaction_begin();
    for (i = 0; i < PAM_REGIONS_COUNT; i++) {
        pam_update(&mch->pam_regions[i], i,
                   pd->config[I855PM_HOST_BRIDGE_PAM0 + DIV_ROUND_UP(i, 2)]);
    }
    memory_region_transaction_commit();
}

/* SMRAM */
static void i855pm_mch_update_smram(I855PMMCHState *mch)
{
    PCIDevice *pd = PCI_DEVICE(mch);
    uint8_t smram = pd->config[I855PM_HOST_BRIDGE_SMRAM];
    uint8_t esmramc = pd->config[I855PM_HOST_BRIDGE_ESMRAMC];
    bool h_smrame = esmramc & I855PM_HOST_BRIDGE_ESMRAMC_H_SMRAME;
    bool g_smrame = smram & I855PM_HOST_BRIDGE_SMRAM_G_SMRAME;
    uint32_t tseg_size = 0;

    if (smram & I855PM_HOST_BRIDGE_SMRAM_D_LCK) {
        pd->config[I855PM_HOST_BRIDGE_SMRAM] &= ~I855PM_HOST_BRIDGE_SMRAM_D_OPEN;
        pd->wmask[I855PM_HOST_BRIDGE_SMRAM] =
            I855PM_HOST_BRIDGE_SMRAM_WMASK_LCK;
        pd->wmask[I855PM_HOST_BRIDGE_ESMRAMC] = 0;
        smram = pd->config[I855PM_HOST_BRIDGE_SMRAM];
    }

    memory_region_transaction_begin();

    if (smram & I855PM_HOST_BRIDGE_SMRAM_D_OPEN) {
        memory_region_set_enabled(&mch->smram_region, h_smrame);
        memory_region_set_enabled(&mch->open_high_smram, h_smrame);
    } else {
        memory_region_set_enabled(&mch->smram_region, true);
        memory_region_set_enabled(&mch->open_high_smram, false);
    }

    memory_region_set_enabled(&mch->low_smram, g_smrame && !h_smrame);
    memory_region_set_enabled(&mch->high_smram, g_smrame && h_smrame);

    if (g_smrame &&
        (esmramc & I855PM_HOST_BRIDGE_ESMRAMC_T_EN)) {
        switch (esmramc & I855PM_HOST_BRIDGE_ESMRAMC_TSEG_SZ_MASK) {
        case I855PM_HOST_BRIDGE_ESMRAMC_TSEG_SZ_1MB:
            tseg_size = 1 * MiB;
            break;
        case I855PM_HOST_BRIDGE_ESMRAMC_TSEG_SZ_2MB:
            tseg_size = 2 * MiB;
            break;
        case I855PM_HOST_BRIDGE_ESMRAMC_TSEG_SZ_8MB:
            tseg_size = 8 * MiB;
            break;
        default:
            /* Reserved encoding: leave TSEG disabled. */
            break;
        }
    }

    memory_region_del_subregion(mch->system_memory, &mch->tseg_blackhole);
    memory_region_set_enabled(&mch->tseg_blackhole, tseg_size != 0);
    memory_region_set_size(&mch->tseg_blackhole, tseg_size ?: 1);
    memory_region_add_subregion_overlap(mch->system_memory,
                                        mch->below_4g_mem_size - tseg_size,
                                        &mch->tseg_blackhole, 2);

    memory_region_set_enabled(&mch->tseg_window, tseg_size != 0);
    memory_region_set_size(&mch->tseg_window, tseg_size ?: 1);
    memory_region_set_address(&mch->tseg_window,
                              mch->below_4g_mem_size - tseg_size);
    memory_region_set_alias_offset(&mch->tseg_window,
                                   mch->below_4g_mem_size - tseg_size);

    memory_region_transaction_commit();

    if (getenv("M200_SMRAM_TRACE")) {
        fprintf(stderr,
                "[I855PM SMRAM] SMRAM=%02x ESMRAMC=%02x normal-high=%d "
                "smm-low=%d smm-high=%d tseg=%u base=%08" PRIx64 "\n",
                smram, esmramc,
                !!(smram & I855PM_HOST_BRIDGE_SMRAM_D_OPEN) && h_smrame,
                g_smrame && !h_smrame, g_smrame && h_smrame, tseg_size,
                tseg_size ? mch->below_4g_mem_size - tseg_size : 0);
    }
}

static void i855pm_mch_write_config(PCIDevice *d,
                                    uint32_t address, uint32_t val, int len)
{
    I855PMMCHState *mch = I855PM_MCH_DEVICE(d);
    fprintf(stderr, "[M200 MCH CFG W] 00:00.0 offset=0x%02x val=0x%x len=%d (CS:EIP=%04x:%08x base=%08x)\n",
            address, val, len,
            pc_m200_get_cpu_cs(),
            pc_m200_get_cpu_eip(),
            pc_m200_get_cpu_cs_base());

    pci_default_write_config(d, address, val, len);

    if (ranges_overlap(address, len, I855PM_HOST_BRIDGE_PAM0,
                       I855PM_HOST_BRIDGE_PAM_SIZE)) {
        i855pm_mch_update_pam(mch);
    }

    if (ranges_overlap(address, len, I855PM_HOST_BRIDGE_SMRAM, 2)) {
        i855pm_mch_update_smram(mch);
    }
}

static void i855pm_mch_update(I855PMMCHState *mch)
{
    i855pm_mch_update_pam(mch);
    i855pm_mch_update_smram(mch);

    /*
     * pci hole goes from end-of-low-ram (enforcing 2GB minimum base to prevent
     * collisions with low DRAM and match authentic 855PM hardware) to io-apic.
     */
    uint64_t pci_hole_start = MAX((uint64_t)mch->below_4g_mem_size, 0x80000000ULL);
    range_set_bounds(&mch->pci_hole,
                     pci_hole_start,
                     IO_APIC_DEFAULT_ADDRESS - 1);
}

static int i855pm_mch_post_load(void *opaque, int version_id)
{
    I855PMMCHState *mch = opaque;
    i855pm_mch_update(mch);
    return 0;
}

static const VMStateDescription vmstate_i855pm_mch = {
    .name = "i855pm_mch",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = i855pm_mch_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj, I855PMMCHState),
        VMSTATE_END_OF_LIST()
    }
};

static void i855pm_mch_reset(DeviceState *qdev)
{
    PCIDevice *d = PCI_DEVICE(qdev);
    I855PMMCHState *mch = I855PM_MCH_DEVICE(d);
    uint8_t drb_step = (uint8_t)(mch->below_4g_mem_size / (32 * MiB));

    /* Initialize DRB (DRAM Row Boundary) registers */
    d->config[I855PM_HOST_BRIDGE_DRB]     = MIN(drb_step / 2, 0xff);
    d->config[I855PM_HOST_BRIDGE_DRB + 1] = MIN(drb_step, 0xff);
    d->config[I855PM_HOST_BRIDGE_DRB + 2] = MIN(drb_step, 0xff);
    d->config[I855PM_HOST_BRIDGE_DRB + 3] = MIN(drb_step, 0xff);
    memset(d->wmask + I855PM_HOST_BRIDGE_DRB, 0xff, I855PM_HOST_BRIDGE_DRB_SIZE);

    /* Initialize DRA (DRAM Row Attribute) */
    d->config[I855PM_HOST_BRIDGE_DRA] = 0x04;
    d->config[I855PM_HOST_BRIDGE_DRA + 1] = 0x04;
    memset(d->wmask + I855PM_HOST_BRIDGE_DRA, 0xff, I855PM_HOST_BRIDGE_DRA_SIZE);

    /* TOM is derived from installed low memory; 2 GiB therefore reads 8000h.
     * NoteBIOS writes to TOM during memory sizing, so wmask must be 0xffff. */
    pci_set_word(d->config + I855PM_HOST_BRIDGE_TOM,
                 MIN(mch->below_4g_mem_size >> 16, UINT16_MAX));
    pci_set_word(d->wmask + I855PM_HOST_BRIDGE_TOM, 0xffff);

    /* Initialize PAM registers and write masks (Default 0x00: Reads from ROM) */
    memset(d->config + I855PM_HOST_BRIDGE_PAM0, 0x00, I855PM_HOST_BRIDGE_PAM_SIZE);
    memset(d->wmask + I855PM_HOST_BRIDGE_PAM0, 0xff, I855PM_HOST_BRIDGE_PAM_SIZE);

    /* Initialize SMRAM and ESMRAMC */
    d->config[I855PM_HOST_BRIDGE_SMRAM] = I855PM_HOST_BRIDGE_SMRAM_DEFAULT;
    d->wmask[I855PM_HOST_BRIDGE_SMRAM] = I855PM_HOST_BRIDGE_SMRAM_WMASK;
    d->config[I855PM_HOST_BRIDGE_ESMRAMC] =
        I855PM_HOST_BRIDGE_ESMRAMC_DEFAULT;
    d->wmask[I855PM_HOST_BRIDGE_ESMRAMC] =
        I855PM_HOST_BRIDGE_ESMRAMC_WMASK;

    d->config[I855PM_HOST_BRIDGE_APSIZE] = 0x00;
    d->wmask[I855PM_HOST_BRIDGE_APSIZE] = 0x00;
    pci_set_long(d->config + I855PM_HOST_BRIDGE_ATTBASE, 0);
    pci_set_long(d->wmask + I855PM_HOST_BRIDGE_ATTBASE, 0xfffff000);
    pci_set_long(d->config + I855PM_HOST_BRIDGE_AGPCTRL, 0);
    pci_set_long(d->wmask + I855PM_HOST_BRIDGE_AGPCTRL, 0xffffffff);

    i855pm_mch_update(mch);
}

static void i855pm_mch_realize(PCIDevice *d, Error **errp)
{
    int i;
    I855PMMCHState *mch = I855PM_MCH_DEVICE(d);

    /* setup pci memory mapping */
    pc_pci_as_mapping_init(mch->system_memory, mch->pci_address_space);

    /* Set Toshiba Subsystem Vendor & Subsystem Device IDs */
    pci_set_word(d->config + PCI_SUBSYSTEM_VENDOR_ID, PCI_VENDOR_ID_TOSHIBA);
    pci_set_word(d->config + PCI_SUBSYSTEM_ID, 0x0001);

    /* PAM */
    init_pam(&mch->pam_regions[0], OBJECT(mch), mch->ram_memory,
             mch->system_memory, mch->pci_address_space,
             PAM_BIOS_BASE, PAM_BIOS_SIZE);
    for (i = 0; i < ARRAY_SIZE(mch->pam_regions) - 1; ++i) {
        init_pam(&mch->pam_regions[i + 1], OBJECT(mch), mch->ram_memory,
                 mch->system_memory, mch->pci_address_space,
                 PAM_EXPAN_BASE + i * PAM_EXPAN_SIZE, PAM_EXPAN_SIZE);
    }

    /* Compatibility SMRAM, visible to non-SMM software only while open. */
    memory_region_init_alias(&mch->smram_region, OBJECT(mch), "smram-region",
                             mch->pci_address_space,
                             I855PM_HOST_BRIDGE_SMRAM_C_BASE,
                             I855PM_HOST_BRIDGE_SMRAM_C_SIZE);
    memory_region_add_subregion_overlap(mch->system_memory,
                                        I855PM_HOST_BRIDGE_SMRAM_C_BASE,
                                        &mch->smram_region, 1);
    memory_region_set_enabled(&mch->smram_region, false);

    /* Fixed high SMRAM is a dedicated 128 KiB store, not TSEG. */
    memory_region_init_ram(&mch->high_smram_backing, OBJECT(mch),
                           "i855pm.fixed-high-smram", I855PM_HIGH_SMRAM_SIZE,
                           errp);
    if (*errp) {
        return;
    }
    memory_region_init_alias(&mch->open_high_smram, OBJECT(mch),
                             "i855pm.fixed-high-smram-open",
                             &mch->high_smram_backing, 0,
                             I855PM_HIGH_SMRAM_SIZE);
    memory_region_add_subregion_overlap(mch->system_memory,
                                        I855PM_HIGH_SMRAM_BASE,
                                        &mch->open_high_smram, 1000);
    memory_region_set_enabled(&mch->open_high_smram, false);

    /* Higher-priority overlays selected only for CPU accesses in SMM. */
    memory_region_init(&mch->smram, OBJECT(mch), "i855pm.smram", 4 * GiB);
    memory_region_init_alias(&mch->low_smram, OBJECT(mch),
                             "i855pm.compat-smram-smm", mch->ram_memory,
                             I855PM_HOST_BRIDGE_SMRAM_C_BASE,
                             I855PM_HOST_BRIDGE_SMRAM_C_SIZE);
    memory_region_add_subregion(&mch->smram,
                                I855PM_HOST_BRIDGE_SMRAM_C_BASE,
                                &mch->low_smram);
    memory_region_init_alias(&mch->high_smram, OBJECT(mch),
                             "i855pm.fixed-high-smram-smm",
                             &mch->high_smram_backing, 0,
                             I855PM_HIGH_SMRAM_SIZE);
    memory_region_add_subregion(&mch->smram, I855PM_HIGH_SMRAM_BASE,
                                &mch->high_smram);

    /* Toshiba's relocation stub makes FEDB0000 visible at default SMBASE. */
    memory_region_init_alias(&mch->default_smbase, OBJECT(mch),
                             "m200.smm-default-smbase",
                             &mch->high_smram_backing, 0x10000, 0x10000);
    memory_region_add_subregion_overlap(&mch->smram, 0x30000,
                                        &mch->default_smbase, 2);

    /* Top-of-DRAM TSEG is separate and aliases reserved system DRAM. */
    memory_region_init_io(&mch->tseg_blackhole, OBJECT(mch),
                          &i855pm_blackhole_ops, NULL,
                          "i855pm.tseg-blackhole", 1);
    memory_region_set_enabled(&mch->tseg_blackhole, false);
    memory_region_add_subregion_overlap(mch->system_memory,
                                        mch->below_4g_mem_size,
                                        &mch->tseg_blackhole, 2);
    memory_region_init_alias(&mch->tseg_window, OBJECT(mch),
                             "i855pm.tseg-smm", mch->ram_memory,
                             mch->below_4g_mem_size, 1);
    memory_region_set_enabled(&mch->tseg_window, false);
    memory_region_add_subregion(&mch->smram, mch->below_4g_mem_size,
                                &mch->tseg_window);

    object_property_add_const_link(qdev_get_machine(), "smram",
                                   OBJECT(&mch->smram));

    /* APBASE is a 256 MiB prefetchable aperture; GART remains inactive. */
    memory_region_init(&mch->agp_aperture, OBJECT(mch),
                       "i855pm.agp-aperture", 256 * MiB);
    pci_register_bar(d, 0, PCI_BASE_ADDRESS_MEM_PREFETCH,
                     &mch->agp_aperture);

    if (pci_add_capability(d, PCI_CAP_ID_AGP, I855PM_AGP_CAP_OFFSET,
                           12, errp) < 0) {
        return;
    }
    pci_set_word(d->config + I855PM_AGP_CAP_OFFSET + 2,
                 I855PM_AGP_CAP_VERSION_2);
    pci_set_long(d->config + I855PM_AGP_CAP_OFFSET + 4,
                 I855PM_AGP_STATUS);
    pci_set_long(d->wmask + I855PM_AGP_CAP_OFFSET + 8,
                 I855PM_AGP_COMMAND_WMASK);

    /* The prior synthetic 0x80000000 MCHBAR was not an 855PM register BAR. */
}

static void i855pm_mch_class_init(ObjectClass *klass, const void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->realize = i855pm_mch_realize;
    k->config_write = i855pm_mch_write_config;
    device_class_set_legacy_reset(dc, i855pm_mch_reset);
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->desc = "i855PM Memory Controller Hub";
    dc->vmsd = &vmstate_i855pm_mch;
    k->vendor_id = PCI_VENDOR_ID_INTEL;
    k->device_id = PCI_DEVICE_ID_INTEL_82855PM_HB;
    k->revision = I855PM_HOST_BRIDGE_REVISION_DEFAULT;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    dc->user_creatable = false;
}

static const TypeInfo i855pm_mch_info = {
    .name = TYPE_I855PM_MCH_DEVICE,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(I855PMMCHState),
    .class_init = i855pm_mch_class_init,
    .interfaces = (const InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void i855pm_register(void)
{
    type_register_static(&i855pm_mch_info);
    type_register_static(&i855pm_agp_info);
    type_register_static(&i855pm_host_info);
}

type_init(i855pm_register);
