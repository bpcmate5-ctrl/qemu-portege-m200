/*
 * i855pm.h - Intel i855PM Memory Controller Hub
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

#ifndef HW_I855PM_H
#define HW_I855PM_H

#include "hw/pci/pci_device.h"
#include "hw/pci/pci_bridge.h"
#include "hw/pci-host/pam.h"
#include "qemu/units.h"
#include "qemu/range.h"
#include "qom/object.h"

#define TYPE_I855PM_HOST_DEVICE "i855pm-pcihost"
OBJECT_DECLARE_SIMPLE_TYPE(I855PMHost, I855PM_HOST_DEVICE)

#define TYPE_I855PM_MCH_DEVICE "i855pm-mch"
OBJECT_DECLARE_SIMPLE_TYPE(I855PMMCHState, I855PM_MCH_DEVICE)

#define TYPE_I855PM_AGP_DEVICE "i855pm-agp"
OBJECT_DECLARE_SIMPLE_TYPE(I855PMAGPState, I855PM_AGP_DEVICE)

struct I855PMMCHState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    MemoryRegion *ram_memory;
    MemoryRegion *pci_address_space;
    MemoryRegion *system_memory;
    MemoryRegion *address_space_io;
    PAMMemoryRegion pam_regions[PAM_REGIONS_COUNT];
    MemoryRegion smram_region;
    MemoryRegion open_high_smram;
    MemoryRegion smram;
    MemoryRegion low_smram;
    MemoryRegion high_smram;
    MemoryRegion high_smram_backing;
    MemoryRegion default_smbase;
    MemoryRegion tseg_blackhole;
    MemoryRegion tseg_window;
    MemoryRegion agp_aperture;
    Range pci_hole;
    uint64_t below_4g_mem_size;
    uint64_t above_4g_mem_size;
};

struct I855PMAGPState {
    /*< private >*/
    PCIBridge parent_obj;
    /*< public >*/
};

struct I855PMHost {
    /*< private >*/
    PCIHostState parent_obj;
    /*< public >*/

    I855PMMCHState mch;
    I855PMAGPState agp;
};

/* i855PM Host Bridge Configuration Space */
#define I855PM_HOST_BRIDGE_CONFIG_ADDR    0xcf8
#define I855PM_HOST_BRIDGE_CONFIG_DATA    0xcfc

#define I855PM_HOST_BRIDGE_REVISION_DEFAULT  0x21
#define I855PM_AGP_BRIDGE_REVISION_DEFAULT   0x21

/* PAM Registers (0x90 - 0x96) */
#define I855PM_HOST_BRIDGE_PAM_NB         7
#define I855PM_HOST_BRIDGE_PAM_SIZE       7
#define I855PM_HOST_BRIDGE_PAM0           0x90
#define I855PM_HOST_BRIDGE_PAM_BIOS_AREA  0xf0000
#define I855PM_HOST_BRIDGE_PAM_AREA_SIZE  0x10000
#define I855PM_HOST_BRIDGE_PAM1           0x91
#define I855PM_HOST_BRIDGE_PAM_EXPAN_AREA 0xc0000
#define I855PM_HOST_BRIDGE_PAM_EXPAN_SIZE 0x04000
#define I855PM_HOST_BRIDGE_PAM2           0x92
#define I855PM_HOST_BRIDGE_PAM3           0x93
#define I855PM_HOST_BRIDGE_PAM4           0x94
#define I855PM_HOST_BRIDGE_PAM_EXBIOS_AREA 0xe0000
#define I855PM_HOST_BRIDGE_PAM_EXBIOS_SIZE 0x04000
#define I855PM_HOST_BRIDGE_PAM5           0x95
#define I855PM_HOST_BRIDGE_PAM6           0x96

/* AGP Configuration Register */
#define I855PM_HOST_BRIDGE_AGPBCTRL       0x70
#define I855PM_HOST_BRIDGE_AGPBCTRL_SIZE  4

/* AGP aperture/GART registers.  Translation is disabled until configured. */
#define I855PM_HOST_BRIDGE_APSIZE         0xb4
#define I855PM_HOST_BRIDGE_ATTBASE        0xb8
#define I855PM_HOST_BRIDGE_AGPCTRL        0xb0

#define I855PM_AGP_CAP_OFFSET             0xa0
#define I855PM_AGP_CAP_VERSION_2          0x0020
#define I855PM_AGP_STATUS                 0x1f000217
#define I855PM_AGP_COMMAND_WMASK          0x1f000317

/* DRAM Controller Registers */
#define I855PM_HOST_BRIDGE_DRB            0x60  /* DRAM Row Boundary (0x60 - 0x63) */
#define I855PM_HOST_BRIDGE_DRB_SIZE       4
#define I855PM_HOST_BRIDGE_DRA            0x70  /* DRAM Row Attribute */
#define I855PM_HOST_BRIDGE_DRA_SIZE       2
#define I855PM_HOST_BRIDGE_TOM            0xc4  /* Top of memory, 64 KiB units */
#define I855PM_HOST_BRIDGE_FDHC           0x58  /* Fixed DRAM Hole Control */

/* SMRAM Control */
#define I855PM_HOST_BRIDGE_SMRAM          0x9d
#define I855PM_HOST_BRIDGE_SMRAM_SIZE     1
#define I855PM_HOST_BRIDGE_SMRAM_D_OPEN   ((uint8_t)(1 << 6))
#define I855PM_HOST_BRIDGE_SMRAM_D_CLS    ((uint8_t)(1 << 5))
#define I855PM_HOST_BRIDGE_SMRAM_D_LCK    ((uint8_t)(1 << 4))
#define I855PM_HOST_BRIDGE_SMRAM_G_SMRAME ((uint8_t)(1 << 3))
#define I855PM_HOST_BRIDGE_SMRAM_C_BASE   0xa0000
#define I855PM_HOST_BRIDGE_SMRAM_C_END    0xc0000
#define I855PM_HOST_BRIDGE_SMRAM_C_SIZE   0x20000
#define I855PM_HOST_BRIDGE_SMRAM_DEFAULT  0x02
#define I855PM_HOST_BRIDGE_SMRAM_WMASK    0x78
#define I855PM_HOST_BRIDGE_SMRAM_WMASK_LCK I855PM_HOST_BRIDGE_SMRAM_D_CLS

#define I855PM_HOST_BRIDGE_ESMRAMC        0x9e
#define I855PM_HOST_BRIDGE_ESMRAMC_H_SMRAME ((uint8_t)(1 << 7))
#define I855PM_HOST_BRIDGE_ESMRAMC_E_SMERR  ((uint8_t)(1 << 6))
#define I855PM_HOST_BRIDGE_ESMRAMC_SM_CACHE ((uint8_t)(1 << 5))
#define I855PM_HOST_BRIDGE_ESMRAMC_SM_L1    ((uint8_t)(1 << 4))
#define I855PM_HOST_BRIDGE_ESMRAMC_SM_L2    ((uint8_t)(1 << 3))
#define I855PM_HOST_BRIDGE_ESMRAMC_TSEG_SZ_MASK ((uint8_t)(3 << 1))
#define I855PM_HOST_BRIDGE_ESMRAMC_TSEG_SZ_1MB  ((uint8_t)(0 << 1))
#define I855PM_HOST_BRIDGE_ESMRAMC_TSEG_SZ_2MB  ((uint8_t)(1 << 1))
#define I855PM_HOST_BRIDGE_ESMRAMC_TSEG_SZ_8MB  ((uint8_t)(2 << 1))
#define I855PM_HOST_BRIDGE_ESMRAMC_T_EN          ((uint8_t)1)
#define I855PM_HOST_BRIDGE_ESMRAMC_DEFAULT       0x38
#define I855PM_HOST_BRIDGE_ESMRAMC_WMASK         0x87

#define I855PM_HIGH_SMRAM_BASE             0xfeda0000
#define I855PM_HIGH_SMRAM_SIZE             0x20000

#endif /* HW_I855PM_H */
