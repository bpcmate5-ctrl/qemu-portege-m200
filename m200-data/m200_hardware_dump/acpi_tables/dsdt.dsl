/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20200925 (32-bit version)
 * Copyright (c) 2000 - 2020 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of dsdt.dat, Sat Aug 22 21:38:32 2026
 *
 * Original Table Header:
 *     Signature        "DSDT"
 *     Length           0x00004426 (17446)
 *     Revision         0x01 **** 32-bit table (V1), no 64-bit math support
 *     Checksum         0xB4
 *     OEM ID           "TOSHIB"
 *     OEM Table ID     "A0009   "
 *     OEM Revision     0x20040107 (537133319)
 *     Compiler ID      "MSFT"
 *     Compiler Version 0x0100000E (16777230)
 */
DefinitionBlock ("", "DSDT", 1, "TOSHIB", "A0009   ", 0x20040107)
{
    External (_PR_.CPU0, UnknownObj)

    Name (\_S0, Package (0x04)  // _S0_: S0 System State
    {
        0x00, 
        0x00, 
        0x00, 
        0x00
    })
    Name (\_S3, Package (0x04)  // _S3_: S3 System State
    {
        0x05, 
        0x00, 
        0x00, 
        0x00
    })
    Name (\_S4, Package (0x04)  // _S4_: S4 System State
    {
        0x06, 
        0x00, 
        0x00, 
        0x00
    })
    Name (\_S5, Package (0x04)  // _S5_: S5 System State
    {
        0x07, 
        0x00, 
        0x00, 
        0x00
    })
    Scope (\_SB)
    {
        Device (LNKA)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x01)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STAL (\_SB.PCI0.FNC0.IRQA))
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,11}
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRSL (\_SB.PCI0.FNC0.IRQA))
            }

            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.FNC0.IRQA = 0x80
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateWordField (Arg0, 0x01, IRQX)
                FindSetRightBit (IRQX, Local0)
                Local0--
                \_SB.PCI0.FNC0.IRQA = Local0
            }
        }

        Device (LNKB)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x02)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STAL (\_SB.PCI0.FNC0.IRQB))
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,11}
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRSL (\_SB.PCI0.FNC0.IRQB))
            }

            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.FNC0.IRQB = 0x80
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateWordField (Arg0, 0x01, IRQX)
                FindSetRightBit (IRQX, Local0)
                Local0--
                \_SB.PCI0.FNC0.IRQB = Local0
            }
        }

        Device (LNKC)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x03)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STAL (\_SB.PCI0.FNC0.IRQC))
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,11}
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRSL (\_SB.PCI0.FNC0.IRQC))
            }

            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.FNC0.IRQC = 0x80
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateWordField (Arg0, 0x01, IRQX)
                FindSetRightBit (IRQX, Local0)
                Local0--
                \_SB.PCI0.FNC0.IRQC = Local0
            }
        }

        Device (LNKD)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x04)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STAL (\_SB.PCI0.FNC0.IRQD))
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,11}
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRSL (\_SB.PCI0.FNC0.IRQD))
            }

            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.FNC0.IRQD = 0x80
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateWordField (Arg0, 0x01, IRQX)
                FindSetRightBit (IRQX, Local0)
                Local0--
                \_SB.PCI0.FNC0.IRQD = Local0
            }
        }

        Device (LNKE)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x05)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STAL (\_SB.PCI0.FNC0.IRQE))
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,11}
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRSL (\_SB.PCI0.FNC0.IRQE))
            }

            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.FNC0.IRQE = 0x80
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateWordField (Arg0, 0x01, IRQX)
                FindSetRightBit (IRQX, Local0)
                Local0--
                \_SB.PCI0.FNC0.IRQE = Local0
            }
        }

        Device (LNKF)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x06)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STAL (\_SB.PCI0.FNC0.IRQF))
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,11}
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRSL (\_SB.PCI0.FNC0.IRQF))
            }

            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.FNC0.IRQF = 0x80
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateWordField (Arg0, 0x01, IRQX)
                FindSetRightBit (IRQX, Local0)
                Local0--
                \_SB.PCI0.FNC0.IRQF = Local0
            }
        }

        Device (LNKG)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x07)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STAL (\_SB.PCI0.FNC0.IRQG))
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {10}
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRSL (\_SB.PCI0.FNC0.IRQG))
            }

            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.FNC0.IRQG = 0x80
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateWordField (Arg0, 0x01, IRQX)
                FindSetRightBit (IRQX, Local0)
                Local0--
                \_SB.PCI0.FNC0.IRQG = Local0
            }
        }

        Device (LNKH)
        {
            Name (_HID, EisaId ("PNP0C0F") /* PCI Interrupt Link Device */)  // _HID: Hardware ID
            Name (_UID, 0x08)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STAL (\_SB.PCI0.FNC0.IRQH))
            }

            Name (_PRS, ResourceTemplate ()  // _PRS: Possible Resource Settings
            {
                IRQ (Level, ActiveLow, Shared, )
                    {3,4,5,6,7,11}
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRSL (\_SB.PCI0.FNC0.IRQH))
            }

            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                \_SB.PCI0.FNC0.IRQH = 0x80
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                CreateWordField (Arg0, 0x01, IRQX)
                FindSetRightBit (IRQX, Local0)
                Local0--
                \_SB.PCI0.FNC0.IRQH = Local0
            }
        }

        Device (MEM)
        {
            Name (_HID, EisaId ("PNP0C01") /* System Board */)  // _HID: Hardware ID
            Name (_STA, 0x0F)  // _STA: Status
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRS (0x01))
            }

            OperationRegion (SRAM, SystemMemory, 0x000EE800, 0x1800)
            Field (SRAM, AnyAcc, NoLock, Preserve)
            {
                PAR1,   16, 
                PAR2,   16, 
                PAR3,   16, 
                PAR4,   16, 
                PAR5,   16, 
                PAR6,   16
            }

            Field (SRAM, AnyAcc, NoLock, Preserve)
            {
                Offset (0x02), 
                RDID,   32, 
                RDSN,   32, 
                CAPB,   16
            }

            Field (SRAM, AnyAcc, NoLock, Preserve)
            {
                IEAX,   32, 
                IEBX,   32, 
                IECX,   32, 
                IEDX,   32, 
                IESI,   32, 
                IEDI,   32, 
                IEBP,   32, 
                Offset (0x20), 
                OEAX,   32, 
                OEBX,   32, 
                OECX,   32, 
                OEDX,   32, 
                OESI,   32, 
                OEDI,   32, 
                OEBP,   32, 
                Offset (0xFF), 
                ACST,   1, 
                BES1,   1, 
                BES2,   1, 
                Offset (0x100), 
                BMN1,   104, 
                BSN1,   88, 
                BTP1,   72, 
                BPU1,   32, 
                BDC1,   32, 
                BLF1,   32, 
                BTC1,   32, 
                BDV1,   32, 
                BST1,   32, 
                BPR1,   32, 
                BRC1,   32, 
                BPV1,   32, 
                Offset (0x149), 
                BCW1,   32, 
                BCL1,   32, 
                BG11,   32, 
                BG21,   32, 
                BOI1,   8, 
                BRF1,   1, 
                Offset (0x200), 
                BMN2,   104, 
                BSN2,   88, 
                BTP2,   72, 
                BPU2,   32, 
                BDC2,   32, 
                BLF2,   32, 
                BTC2,   32, 
                BDV2,   32, 
                BST2,   32, 
                BPR2,   32, 
                BRC2,   32, 
                BPV2,   32, 
                Offset (0x249), 
                BCW2,   32, 
                BCL2,   32, 
                BG12,   32, 
                BG22,   32, 
                BOI2,   32, 
                Offset (0x300), 
                AC01,   16, 
                AC11,   16, 
                PSV1,   16, 
                CRT1,   16, 
                TMP1,   16, 
                AST1,   16, 
                AC21,   16, 
                AC31,   16, 
                AC02,   16, 
                AC12,   16, 
                PSV2,   16, 
                CRT2,   16, 
                TMP2,   16, 
                AST2,   16, 
                AC22,   16, 
                AC32,   16, 
                AC03,   16, 
                AC13,   16, 
                PSV3,   16, 
                CRT3,   16, 
                TMP3,   16, 
                AST3,   16, 
                AC23,   16, 
                AC33,   16, 
                Offset (0x340), 
                TMPF,   16, 
                Offset (0x3F0), 
                FANH,   1, 
                FANL,   7, 
                TF11,   1, 
                TF21,   1, 
                TF31,   1, 
                    ,   1, 
                TF10,   1, 
                TF20,   1, 
                TF30,   1, 
                Offset (0x3F2), 
                TP11,   1, 
                TP21,   1, 
                TP31,   1, 
                Offset (0x400), 
                GP50,   1, 
                GP51,   1, 
                GP52,   1, 
                GP53,   1, 
                GP54,   1, 
                GP55,   1, 
                GP56,   1, 
                GP57,   1, 
                GP60,   1, 
                GP61,   1, 
                GP62,   1, 
                GP63,   1, 
                GP64,   1, 
                GP65,   1, 
                GP66,   1, 
                GP67,   1, 
                GP70,   1, 
                GP71,   1, 
                GP72,   1, 
                GP73,   1, 
                GP74,   1, 
                GP75,   1, 
                GP76,   1, 
                GP77,   1, 
                WED0,   1, 
                WED1,   1, 
                WED2,   1, 
                WED3,   1, 
                WED4,   1, 
                Offset (0x404), 
                SBL0,   1, 
                SBL1,   1, 
                SBL2,   1, 
                SBL3,   1, 
                SBL4,   1, 
                Offset (0x405), 
                LIDS,   1, 
                VALF,   1, 
                DCST,   1, 
                DOS2,   1, 
                DCKI,   1, 
                DCKF,   1, 
                BT1F,   1, 
                BT2F,   1, 
                NXLA,   1, 
                NXCA,   1, 
                NXTA,   1, 
                NXDA,   1, 
                CTLA,   1, 
                CTCA,   1, 
                CTTA,   1, 
                CTDA,   1, 
                LANA,   1, 
                PNLS,   1, 
                Offset (0x483), 
                GCVS,   8, 
                Offset (0x4C0), 
                PSS0,   16, 
                PSS1,   16, 
                Offset (0x4D0), 
                SYU0,   1, 
                SYU1,   1, 
                SYU2,   1, 
                SYU3,   1, 
                SYU4,   1, 
                SYU5,   1, 
                SYU6,   1, 
                SYU7,   1, 
                RPPC,   1, 
                DNTF,   1, 
                DCKU,   1, 
                DCKD,   1, 
                Offset (0x500), 
                HKCD,   8, 
                Offset (0x502), 
                DLID,   32, 
                DSRN,   32, 
                Offset (0x50E), 
                BDID,   32, 
                DSPW,   1, 
                VGAF,   1, 
                VWE0,   1, 
                VWE1,   1, 
                PPSC,   1, 
                SPSC,   1, 
                EWLD,   1, 
                EPWS,   1, 
                LCDS,   4, 
                CRTS,   4, 
                VWE2,   1, 
                WEF0,   1, 
                WEF1,   1, 
                WED5,   1, 
                IEWE,   1, 
                Offset (0x515), 
                BTMD,   1, 
                WSF0,   1, 
                WSF1,   1, 
                GP83,   1, 
                WUIE,   1, 
                    ,   1, 
                BPFE,   1, 
                BWUE,   1, 
                DVIS,   4, 
                Offset (0x517), 
                HTM0,   1, 
                HTM1,   1, 
                    ,   1, 
                    ,   1, 
                I1EJ,   1, 
                Offset (0x518), 
                PSND,   1, 
                PMDM,   1, 
                Offset (0x520), 
                VGAR,   1, 
                KBCR,   1, 
                ID0R,   1, 
                ID1R,   1, 
                ID2R,   1, 
                ID3R,   1, 
                IDAR,   1, 
                ACLR,   1, 
                BTRE,   1, 
                ACVA,   1, 
                Offset (0x522), 
                GP90,   1, 
                GP91,   1, 
                Offset (0x6C0), 
                BDCS,   1, 
                Offset (0x6C1), 
                BWE0,   1, 
                BWE1,   1, 
                BWE2,   1, 
                BWE3,   1, 
                BWE4,   1, 
                BWE5,   1, 
                BWF0,   1, 
                BWF1,   1, 
                BCDD,   4, 
                Offset (0x701), 
                HAPS,   2, 
                HHSW,   2, 
                HPSU,   2, 
                HRCU,   2, 
                HGSU,   2, 
                HEBI,   2, 
                HTMD,   2, 
                Offset (0x708), 
                TNVS,   1, 
                OSPC,   1, 
                ACBK,   1, 
                LBAT,   1, 
                LANB,   1, 
                Offset (0x70A), 
                PULD,   8, 
                PULA,   8, 
                BCLD,   8, 
                BCLA,   8, 
                Offset (0x710), 
                OSID,   8, 
                Offset (0x720), 
                MSSI,   16, 
                MSSS,   8, 
                MSSR,   8, 
                MSP0,   8, 
                MSC0,   8, 
                MSP1,   8, 
                MSC1,   8, 
                Offset (0x740), 
                Offset (0x800), 
                PRES,   32768
            }

            Field (SRAM, AnyAcc, NoLock, Preserve)
            {
                Offset (0x406), 
                NXDD,   4, 
                CTDD,   4
            }

            Field (SRAM, AnyAcc, NoLock, Preserve)
            {
                Offset (0x800), 
                Offset (0x808), 
                Offset (0x812), 
                Offset (0x814), 
                Offset (0x818), 
                FSDP,   8, 
                Offset (0x823), 
                Offset (0x826), 
                Offset (0x836), 
                Offset (0x87E), 
                Offset (0x87F), 
                EDCK,   8
            }
        }

        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A03") /* PCI Bus */)  // _HID: Hardware ID
            Name (_ADR, 0x00)  // _ADR: Address
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Name (CRBF, ResourceTemplate ()
                {
                    WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,
                        0x0000,             // Granularity
                        0x0000,             // Range Minimum
                        0x00FF,             // Range Maximum
                        0x0000,             // Translation Offset
                        0x0100,             // Length
                        ,, )
                    IO (Decode16,
                        0x0CF8,             // Range Minimum
                        0x0CF8,             // Range Maximum
                        0x01,               // Alignment
                        0x08,               // Length
                        )
                    WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                        0x0000,             // Granularity
                        0x0000,             // Range Minimum
                        0x0CF7,             // Range Maximum
                        0x0000,             // Translation Offset
                        0x0CF8,             // Length
                        ,, , TypeStatic, DenseTranslation)
                    WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                        0x0000,             // Granularity
                        0x0D00,             // Range Minimum
                        0xFFFF,             // Range Maximum
                        0x0000,             // Translation Offset
                        0xF300,             // Length
                        ,, , TypeStatic, DenseTranslation)
                    DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                        0x00000000,         // Granularity
                        0x000A0000,         // Range Minimum
                        0x000BFFFF,         // Range Maximum
                        0x00000000,         // Translation Offset
                        0x00020000,         // Length
                        ,, , AddressRangeMemory, TypeStatic)
                    DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                        0x00000000,         // Granularity
                        0x000D0000,         // Range Minimum
                        0x000DFFFF,         // Range Maximum
                        0x00000000,         // Translation Offset
                        0x00010000,         // Length
                        ,, _Y00, AddressRangeMemory, TypeStatic)
                    DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                        0x00000000,         // Granularity
                        0x80100000,         // Range Minimum
                        0xFED9FFFF,         // Range Maximum
                        0x00000000,         // Translation Offset
                        0x7ECA0000,         // Length
                        ,, , AddressRangeMemory, TypeStatic)
                    DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                        0x00000000,         // Granularity
                        0xFEDC0000,         // Range Minimum
                        0xFFB7FFFF,         // Range Maximum
                        0x00000000,         // Translation Offset
                        0x00DC0000,         // Length
                        ,, , AddressRangeMemory, TypeStatic)
                    DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                        0x00000000,         // Granularity
                        0xFFC00000,         // Range Minimum
                        0xFFEFFFFF,         // Range Maximum
                        0x00000000,         // Translation Offset
                        0x00300000,         // Length
                        ,, , AddressRangeMemory, TypeStatic)
                })
                CreateDWordField (CRBF, \_SB.PCI0._CRS._Y00._MIN, EMMS)  // _MIN: Minimum Base Address
                CreateDWordField (CRBF, \_SB.PCI0._CRS._Y00._LEN, EMME)  // _LEN: Length
                If (\_SB.MEM.LANB)
                {
                    EMMS = 0x000D8000
                    EMME = 0x8000
                }

                Return (CRBF) /* \_SB_.PCI0._CRS.CRBF */
            }

            Name (_PRT, Package (0x06)  // _PRT: PCI Routing Table
            {
                Package (0x04)
                {
                    0x001FFFFF, 
                    0x00, 
                    \_SB.LNKC, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x001FFFFF, 
                    0x01, 
                    \_SB.LNKB, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    0x00, 
                    \_SB.LNKA, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    0x01, 
                    \_SB.LNKD, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    0x02, 
                    \_SB.LNKC, 
                    0x00
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    0x03, 
                    \_SB.LNKH, 
                    0x00
                }
            })
            Device (PCIB)
            {
                Name (_ADR, 0x001E0000)  // _ADR: Address
                OperationRegion (ICHB, PCI_Config, 0x00, 0xFF)
                Field (ICHB, ByteAcc, NoLock, Preserve)
                {
                    Offset (0x19), 
                    BRGB,   8
                }

                Name (_PRT, Package (0x05)  // _PRT: PCI Routing Table
                {
                    Package (0x04)
                    {
                        0x000BFFFF, 
                        0x00, 
                        \_SB.LNKD, 
                        0x00
                    }, 

                    Package (0x04)
                    {
                        0x0005FFFF, 
                        0x00, 
                        \_SB.LNKC, 
                        0x00
                    }, 

                    Package (0x04)
                    {
                        0x0005FFFF, 
                        0x01, 
                        \_SB.LNKA, 
                        0x00
                    }, 

                    Package (0x04)
                    {
                        0x000DFFFF, 
                        0x00, 
                        \_SB.LNKD, 
                        0x00
                    }, 

                    Package (0x04)
                    {
                        0x0008FFFF, 
                        0x00, 
                        \_SB.LNKE, 
                        0x00
                    }
                })
                Device (MPC0)
                {
                    Name (_ADR, 0x00050000)  // _ADR: Address
                }

                Device (MPC1)
                {
                    Name (_ADR, 0x00050001)  // _ADR: Address
                }

                Device (LAN)
                {
                    Name (_ADR, 0x00080000)  // _ADR: Address
                    OperationRegion (PLAN, PCI_Config, 0x00, 0xFF)
                    Field (PLAN, ByteAcc, NoLock, Preserve)
                    {
                        PLVI,   16, 
                        Offset (0xE1), 
                        PMES,   8
                    }

                    Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                    {
                        0x0B, 
                        0x04
                    })
                    Method (_PSW, 1, NotSerialized)  // _PSW: Power State Wake
                    {
                        If (Arg0)
                        {
                            \_SB.MEM.WED5 = 0x01
                        }
                        Else
                        {
                            \_SB.MEM.WED5 = 0x00
                        }
                    }
                }

                Device (VIY0)
                {
                    Name (_ADR, 0x000B0000)  // _ADR: Address
                    Name (_SUN, 0x00)  // _SUN: Slot User Number
                    Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                    {
                        0x0B, 
                        0x03
                    })
                    Method (_PSW, 1, NotSerialized)  // _PSW: Power State Wake
                    {
                        If (Arg0)
                        {
                            \_SB.MEM.VWE0 = 0x01
                        }
                        Else
                        {
                            \_SB.MEM.VWE0 = 0x00
                        }
                    }
                }

                Device (SDC)
                {
                    Name (_ADR, 0x000D0000)  // _ADR: Address
                }

                Event (DKSQ)
                Event (SBSQ)
            }

            Device (FNC0)
            {
                Name (_ADR, 0x001F0000)  // _ADR: Address
                OperationRegion (ICH3, PCI_Config, 0x00, 0xFF)
                Field (ICH3, ByteAcc, NoLock, Preserve)
                {
                    Offset (0x60), 
                    IRQA,   8, 
                    IRQB,   8, 
                    IRQC,   8, 
                    IRQD,   8, 
                    Offset (0x68), 
                    IRQE,   8, 
                    IRQF,   8, 
                    IRQG,   8, 
                    IRQH,   8
                }

                Device (DMAC)
                {
                    Name (_HID, EisaId ("PNP0200") /* PC-class DMA Controller */)  // _HID: Hardware ID
                    Name (_STA, 0x0F)  // _STA: Status
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0000,             // Range Minimum
                            0x0000,             // Range Maximum
                            0x01,               // Alignment
                            0x10,               // Length
                            )
                        IO (Decode16,
                            0x0081,             // Range Minimum
                            0x0081,             // Range Maximum
                            0x01,               // Alignment
                            0x03,               // Length
                            )
                        IO (Decode16,
                            0x0087,             // Range Minimum
                            0x0087,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0089,             // Range Minimum
                            0x0089,             // Range Maximum
                            0x01,               // Alignment
                            0x03,               // Length
                            )
                        IO (Decode16,
                            0x008F,             // Range Minimum
                            0x008F,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x00C0,             // Range Minimum
                            0x00C0,             // Range Maximum
                            0x01,               // Alignment
                            0x20,               // Length
                            )
                        DMA (Compatibility, NotBusMaster, Transfer8, )
                            {4}
                    })
                }

                Device (PIC)
                {
                    Name (_HID, EisaId ("PNP0000") /* 8259-compatible Programmable Interrupt Controller */)  // _HID: Hardware ID
                    Name (_STA, 0x0F)  // _STA: Status
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0020,             // Range Minimum
                            0x0020,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00A0,             // Range Minimum
                            0x00A0,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IRQ (Edge, ActiveHigh, Exclusive, )
                            {2}
                    })
                }

                Device (PIT)
                {
                    Name (_HID, EisaId ("PNP0100") /* PC-class System Timer */)  // _HID: Hardware ID
                    Name (_STA, 0x0F)  // _STA: Status
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0040,             // Range Minimum
                            0x0040,             // Range Maximum
                            0x01,               // Alignment
                            0x04,               // Length
                            )
                        IRQ (Edge, ActiveHigh, Exclusive, )
                            {0}
                    })
                }

                Device (SPKR)
                {
                    Name (_HID, EisaId ("PNP0800") /* Microsoft Sound System Compatible Device */)  // _HID: Hardware ID
                    Name (_STA, 0x0F)  // _STA: Status
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0061,             // Range Minimum
                            0x0061,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                    })
                }

                Device (NDP)
                {
                    Name (_HID, EisaId ("PNP0C04") /* x87-compatible Floating Point Processing Unit */)  // _HID: Hardware ID
                    Name (_STA, 0x0F)  // _STA: Status
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x00F0,             // Range Minimum
                            0x00F0,             // Range Maximum
                            0x01,               // Alignment
                            0x10,               // Length
                            )
                        IRQ (Edge, ActiveHigh, Exclusive, )
                            {13}
                    })
                }

                Device (KBC)
                {
                    Name (_HID, EisaId ("TOS7406"))  // _HID: Hardware ID
                    Name (_CID, EisaId ("PNP0303") /* IBM Enhanced Keyboard (101/102-key, PS/2 Mouse) */)  // _CID: Compatible ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        While ((\_SB.MEM.KBCR == 0x00)) {}
                        Return (0x0F)
                    }

                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0060,             // Range Minimum
                            0x0060,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0064,             // Range Minimum
                            0x0064,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IRQ (Edge, ActiveHigh, Exclusive, )
                            {1}
                    })
                }

                Device (PS2M)
                {
                    Name (_HID, EisaId ("PNP0F13") /* PS/2 Mouse */)  // _HID: Hardware ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        While ((\_SB.MEM.KBCR == 0x00)) {}
                        Return (0x0F)
                    }

                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IRQ (Edge, ActiveHigh, Exclusive, )
                            {12}
                    })
                }

                Device (RTC)
                {
                    Name (_HID, EisaId ("PNP0B00") /* AT Real-Time Clock */)  // _HID: Hardware ID
                    Name (_STA, 0x0F)  // _STA: Status
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0070,             // Range Minimum
                            0x0070,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IRQ (Edge, ActiveHigh, Exclusive, )
                            {8}
                    })
                }

                Device (SYSR)
                {
                    Name (_HID, EisaId ("PNP0C02") /* PNP Motherboard Resources */)  // _HID: Hardware ID
                    Name (_STA, 0x0F)  // _STA: Status
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0062,             // Range Minimum
                            0x0062,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0066,             // Range Minimum
                            0x0066,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0080,             // Range Minimum
                            0x0080,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0084,             // Range Minimum
                            0x0084,             // Range Maximum
                            0x01,               // Alignment
                            0x03,               // Length
                            )
                        IO (Decode16,
                            0x0088,             // Range Minimum
                            0x0088,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x008C,             // Range Minimum
                            0x008C,             // Range Maximum
                            0x01,               // Alignment
                            0x03,               // Length
                            )
                        IO (Decode16,
                            0x00E0,             // Range Minimum
                            0x00E0,             // Range Maximum
                            0x01,               // Alignment
                            0x10,               // Length
                            )
                        IO (Decode16,
                            0x01E0,             // Range Minimum
                            0x01E0,             // Range Maximum
                            0x01,               // Alignment
                            0x08,               // Length
                            )
                        IO (Decode16,
                            0x0480,             // Range Minimum
                            0x0480,             // Range Maximum
                            0x01,               // Alignment
                            0x10,               // Length
                            )
                        IO (Decode16,
                            0x0680,             // Range Minimum
                            0x0680,             // Range Maximum
                            0x80,               // Alignment
                            0x80,               // Length
                            )
                        IO (Decode16,
                            0xD800,             // Range Minimum
                            0xD800,             // Range Maximum
                            0x01,               // Alignment
                            0x80,               // Length
                            )
                        IO (Decode16,
                            0xD880,             // Range Minimum
                            0xD880,             // Range Maximum
                            0x01,               // Alignment
                            0x20,               // Length
                            )
                        IO (Decode16,
                            0xD8A0,             // Range Minimum
                            0xD8A0,             // Range Maximum
                            0x01,               // Alignment
                            0x20,               // Length
                            )
                        IO (Decode16,
                            0xE000,             // Range Minimum
                            0xE000,             // Range Maximum
                            0x01,               // Alignment
                            0x80,               // Length
                            )
                        IO (Decode16,
                            0xE080,             // Range Minimum
                            0xE080,             // Range Maximum
                            0x01,               // Alignment
                            0x80,               // Length
                            )
                        IO (Decode16,
                            0xE400,             // Range Minimum
                            0xE400,             // Range Maximum
                            0x01,               // Alignment
                            0x80,               // Length
                            )
                        IO (Decode16,
                            0xE480,             // Range Minimum
                            0xE480,             // Range Maximum
                            0x01,               // Alignment
                            0x80,               // Length
                            )
                        IO (Decode16,
                            0xE800,             // Range Minimum
                            0xE800,             // Range Maximum
                            0x01,               // Alignment
                            0x80,               // Length
                            )
                        IO (Decode16,
                            0xE880,             // Range Minimum
                            0xE880,             // Range Maximum
                            0x01,               // Alignment
                            0x80,               // Length
                            )
                        IO (Decode16,
                            0xEC00,             // Range Minimum
                            0xEC00,             // Range Maximum
                            0x01,               // Alignment
                            0x80,               // Length
                            )
                        IO (Decode16,
                            0xEC80,             // Range Minimum
                            0xEC80,             // Range Maximum
                            0x01,               // Alignment
                            0x80,               // Length
                            )
                        IO (Decode16,
                            0xEEAC,             // Range Minimum
                            0xEEAC,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0xEEB0,             // Range Minimum
                            0xEEB0,             // Range Maximum
                            0x01,               // Alignment
                            0x10,               // Length
                            )
                        IO (Decode16,
                            0xEEC0,             // Range Minimum
                            0xEEC0,             // Range Maximum
                            0x01,               // Alignment
                            0x40,               // Length
                            )
                        IO (Decode16,
                            0x0010,             // Range Minimum
                            0x0010,             // Range Maximum
                            0x01,               // Alignment
                            0x10,               // Length
                            )
                        IO (Decode16,
                            0x0024,             // Range Minimum
                            0x0024,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0028,             // Range Minimum
                            0x0028,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x002C,             // Range Minimum
                            0x002C,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0030,             // Range Minimum
                            0x0030,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0034,             // Range Minimum
                            0x0034,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0038,             // Range Minimum
                            0x0038,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x003C,             // Range Minimum
                            0x003C,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0050,             // Range Minimum
                            0x0050,             // Range Maximum
                            0x01,               // Alignment
                            0x04,               // Length
                            )
                        IO (Decode16,
                            0x0063,             // Range Minimum
                            0x0063,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0065,             // Range Minimum
                            0x0065,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0072,             // Range Minimum
                            0x0072,             // Range Maximum
                            0x01,               // Alignment
                            0x06,               // Length
                            )
                        IO (Decode16,
                            0x0090,             // Range Minimum
                            0x0090,             // Range Maximum
                            0x01,               // Alignment
                            0x10,               // Length
                            )
                        IO (Decode16,
                            0x00A4,             // Range Minimum
                            0x00A4,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00A8,             // Range Minimum
                            0x00A8,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00AC,             // Range Minimum
                            0x00AC,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00B0,             // Range Minimum
                            0x00B0,             // Range Maximum
                            0x01,               // Alignment
                            0x06,               // Length
                            )
                        IO (Decode16,
                            0x00B8,             // Range Minimum
                            0x00B8,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00BC,             // Range Minimum
                            0x00BC,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x04D0,             // Range Minimum
                            0x04D0,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                    })
                    OperationRegion (SRG1, SystemIO, 0xB2, 0x01)
                    Field (SRG1, ByteAcc, NoLock, Preserve)
                    {
                        TRP4,   8
                    }
                }

                Device (TBLT)
                {
                    Name (_HID, EisaId ("WACF004"))  // _HID: Hardware ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        Return (STA (0x0E))
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Return (CRS (0x0E))
                    }

                    Method (_PRS, 0, NotSerialized)  // _PRS: Possible Resource Settings
                    {
                        Return (PRS (0x0E))
                    }

                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        SRS (0x0E, Arg0)
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        DIS (0x0E)
                    }
                }

                Device (FSIR)
                {
                    Name (_HID, EisaId ("SMCF030"))  // _HID: Hardware ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        Return (STA (0x0F))
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Return (CRS (0x0F))
                    }

                    Method (_PRS, 0, NotSerialized)  // _PRS: Possible Resource Settings
                    {
                        Return (PRS (0x0F))
                    }

                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        SRS (0x0F, Arg0)
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        DIS (0x0F)
                    }
                }

                Device (PCC0)
                {
                    Name (_HID, EisaId ("PNP0E00"))  // _HID: Hardware ID
                    Name (_UID, 0x01)  // _UID: Unique ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        Return (STA (0x13))
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Return (CRS (0x13))
                    }

                    Method (_PRS, 0, NotSerialized)  // _PRS: Possible Resource Settings
                    {
                        Return (PRS (0x13))
                    }

                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        SRS (0x13, Arg0)
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        DIS (0x13)
                    }

                    Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                    {
                        0x0B, 
                        0x03
                    })
                    Method (_PSW, 1, NotSerialized)  // _PSW: Power State Wake
                    {
                        \_SB.MEM.WED2 = Arg0
                    }

                    Device (PCS0)
                    {
                        Name (_ADR, 0x00)  // _ADR: Address
                        Name (_SUN, 0x00)  // _SUN: Slot User Number
                    }
                }

                Device (ATA)
                {
                    Name (_HID, EisaId ("PNP0600"))  // _HID: Hardware ID
                    Name (_UID, 0x03)  // _UID: Unique ID
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        Return (STA (0x16))
                    }

                    Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                    {
                        Return (CRS (0x16))
                    }

                    Method (_PRS, 0, NotSerialized)  // _PRS: Possible Resource Settings
                    {
                        Return (PRS (0x16))
                    }

                    Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
                    {
                        SRS (0x16, Arg0)
                    }

                    Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
                    {
                        DIS (0x16)
                    }
                }
            }

            Device (FNC1)
            {
                Name (_ADR, 0x001F0001)  // _ADR: Address
                OperationRegion (IDEC, PCI_Config, 0x00, 0xFF)
                Field (IDEC, ByteAcc, NoLock, Preserve)
                {
                    Offset (0x40), 
                    PRTE,   8, 
                    PRTM,   8, 
                    SRTE,   8, 
                    SRTM,   8, 
                    Offset (0x48), 
                    PUDM,   1, 
                        ,   1, 
                    SUDM,   1, 
                    Offset (0x4A), 
                    PUDC,   2, 
                    Offset (0x4B), 
                    SUDC,   2, 
                    Offset (0x54), 
                    PCB0,   1, 
                        ,   1, 
                    SCB0,   1, 
                        ,   1, 
                    PCS0,   1, 
                    PCS1,   1, 
                    SCS0,   1, 
                    SCS1,   1, 
                        ,   2, 
                    PP_E,   1, 
                        ,   1, 
                    FPC0,   1, 
                        ,   1, 
                    FSC0,   1
                }

                Field (IDEC, ByteAcc, NoLock, Preserve)
                {
                    Offset (0x54), 
                        ,   4, 
                    CSTS,   4
                }

                Device (IDE0)
                {
                    Name (_ADR, 0x00)  // _ADR: Address
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        Return (0x0F)
                    }

                    Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                    {
                        \_SB.MEM.PPSC = 0x00
                    }

                    Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                    {
                        \_SB.MEM.PPSC = 0x01
                    }

                    Method (_PSC, 0, NotSerialized)  // _PSC: Power State Current
                    {
                        If (\_SB.MEM.PPSC)
                        {
                            Return (0x03)
                        }
                        Else
                        {
                            Return (0x00)
                        }
                    }

                    Method (_STM, 3, NotSerialized)  // _STM: Set Timing Mode
                    {
                        While ((\_SB.MEM.ID0R == 0x00)) {}
                        \_SB.MEM.HTM0 = 0x01
                        CreateDWordField (Arg0, 0x00, PPIO)
                        CreateDWordField (Arg0, 0x04, PDMA)
                        CreateDWordField (Arg0, 0x10, PFLG)
                        Local1 = PPIO /* \_SB_.PCI0.FNC1.IDE0._STM.PPIO */
                        Local2 = PDMA /* \_SB_.PCI0.FNC1.IDE0._STM.PDMA */
                        Local3 = PFLG /* \_SB_.PCI0.FNC1.IDE0._STM.PFLG */
                        Local4 = 0x80
                        Local7 = 0x00
                        If ((Local1 <= 0x78))
                        {
                            Local4 = 0xA3
                            Local7 = 0x03
                        }
                        ElseIf ((Local1 <= 0xB4))
                        {
                            Local4 = 0xA1
                            Local7 = 0x03
                        }
                        ElseIf ((Local1 <= 0xF0))
                        {
                            Local4 = 0x90
                            Local7 = 0x01
                        }

                        \_SB.PCI0.FNC1.PRTM = Local4
                        Local4 = 0x00
                        Local5 = 0x00
                        Local6 = 0x00
                        Local1 = 0x00
                        Local3 &= 0x01
                        \_SB.PCI0.FNC1.PUDM = Local3
                        If ((Local3 == 0x01))
                        {
                            Local7 = 0x07
                            Local1 = 0x01
                            If ((Local2 <= 0x14))
                            {
                                Local6 = 0x01
                                Local4 = 0x01
                            }
                            ElseIf ((Local2 <= 0x1E))
                            {
                                Local4 = 0x02
                                Local5 = 0x01
                            }
                            ElseIf ((Local2 <= 0x3C))
                            {
                                Local4 = 0x02
                            }
                            ElseIf ((Local2 <= 0x5A))
                            {
                                Local4 = 0x01
                            }
                        }

                        \_SB.PCI0.FNC1.PUDC = Local4
                        \_SB.PCI0.FNC1.PCB0 = Local5
                        \_SB.PCI0.FNC1.FPC0 = Local6
                        \_SB.PCI0.FNC1.PRTE = Local7
                        \_SB.PCI0.FNC1.PCS0 = Local1
                        Local1 = 0x01
                        Local0 = \_SB.PCI0.FNC1.CSTS
                        If ((Local0 == 0x00))
                        {
                            Local1 = 0x00
                        }

                        \_SB.PCI0.FNC1.PP_E = Local1
                    }

                    Method (_GTM, 0, NotSerialized)  // _GTM: Get Timing Mode
                    {
                        While ((\_SB.MEM.ID0R == 0x00)) {}
                        \_SB.MEM.HTM0 = 0x01
                        Local0 = \_SB.PCI0.FNC1.PRTM
                        Local2 = 0xFA
                        If ((Local0 == 0xA3))
                        {
                            Local2 = 0x78
                        }
                        ElseIf ((Local0 == 0xA1))
                        {
                            Local2 = 0xB4
                        }
                        ElseIf ((Local0 == 0x90))
                        {
                            Local2 = 0xF0
                        }

                        Local1 = \_SB.PCI0.FNC1.PUDC
                        Local4 = 0x02
                        If ((\_SB.PCI0.FNC1.PUDM == 0x01))
                        {
                            Local4 = 0x03
                            If ((\_SB.PCI0.FNC1.FPC0 == 0x01))
                            {
                                Local3 = 0x14
                            }
                            ElseIf ((\_SB.PCI0.FNC1.PCB0 == 0x01))
                            {
                                Local3 = 0x1E
                            }
                            Else
                            {
                                Local3 = 0x78
                                If ((Local1 == 0x01))
                                {
                                    Local3 = 0x5A
                                }
                                ElseIf ((Local1 == 0x02))
                                {
                                    Local3 = 0x3C
                                }
                            }
                        }
                        Else
                        {
                            Local3 = 0xB4
                            If ((Local0 == 0xA3))
                            {
                                Local3 = 0x78
                            }
                        }

                        Name (BUFF, Buffer (0x14) {})
                        CreateDWordField (BUFF, 0x00, PIO1)
                        CreateDWordField (BUFF, 0x04, DMA1)
                        CreateDWordField (BUFF, 0x08, PIO2)
                        CreateDWordField (BUFF, 0x0C, DMA2)
                        CreateDWordField (BUFF, 0x10, FLGS)
                        PIO1 = Local2
                        DMA1 = Local3
                        PIO2 = 0xFFFFFFFF
                        DMA2 = 0xFFFFFFFF
                        FLGS = Local4
                        Return (BUFF) /* \_SB_.PCI0.FNC1.IDE0._GTM.BUFF */
                    }

                    Device (HD_0)
                    {
                        Name (_ADR, 0x00)  // _ADR: Address
                        Method (_GTF, 0, NotSerialized)  // _GTF: Get Task File
                        {
                            While ((\_SB.MEM.ID0R == 0x00)) {}
                            \_SB.MEM.HTM0 = 0x01
                            Name (BUFF, Buffer (0x0E)
                            {
                                /* 0000 */  0x03, 0x0C, 0x00, 0x00, 0x00, 0x00, 0xEF, 0x03,  // ........
                                /* 0008 */  0x23, 0x00, 0x00, 0x00, 0x00, 0xEF               // #.....
                            })
                            CreateByteField (BUFF, 0x01, PIOM)
                            CreateByteField (BUFF, 0x08, DMAM)
                            Local0 = \_SB.PCI0.FNC1.PRTM
                            Local1 = 0x08
                            If ((Local0 == 0xA3))
                            {
                                Local1 = 0x0C
                            }
                            ElseIf ((Local0 == 0xA1))
                            {
                                Local1 = 0x0B
                            }
                            ElseIf ((Local0 == 0x90))
                            {
                                Local1 = 0x0A
                            }

                            Local2 = \_SB.PCI0.FNC1.PUDM
                            If ((Local2 == 0x01))
                            {
                                Local4 = \_SB.PCI0.FNC1.PUDC
                                If ((\_SB.PCI0.FNC1.FPC0 == 0x01))
                                {
                                    Local3 = 0x45
                                }
                                ElseIf ((\_SB.PCI0.FNC1.PCB0 == 0x01))
                                {
                                    Local3 = 0x44
                                }
                                Else
                                {
                                    Local3 = 0x40
                                    If ((Local4 == 0x01))
                                    {
                                        Local3 = 0x41
                                    }
                                    ElseIf ((Local4 == 0x02))
                                    {
                                        Local3 = 0x42
                                    }
                                }
                            }
                            Else
                            {
                                Local3 = 0x21
                                If ((Local0 == 0xA3))
                                {
                                    Local3 = 0x22
                                }
                            }

                            PIOM = Local1
                            DMAM = Local3
                            Return (BUFF) /* \_SB_.PCI0.FNC1.IDE0.HD_0._GTF.BUFF */
                        }
                    }
                }
            }

            Device (USB1)
            {
                Name (_ADR, 0x001D0000)  // _ADR: Address
                Device (HUB1)
                {
                    Name (_ADR, 0x00)  // _ADR: Address
                    Device (POT1)
                    {
                        Name (_ADR, 0x01)  // _ADR: Address
                    }

                    Device (POT2)
                    {
                        Name (_ADR, 0x02)  // _ADR: Address
                    }
                }

                Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                {
                    0x03, 
                    0x03
                })
                Method (_PSW, 1, NotSerialized)  // _PSW: Power State Wake
                {
                    If (Arg0)
                    {
                        \_SB.MEM.WED3 = 0x01
                    }
                    Else
                    {
                        \_SB.MEM.WED3 = 0x00
                    }
                }
            }

            Device (USB2)
            {
                Name (_ADR, 0x001D0001)  // _ADR: Address
                Device (HUB2)
                {
                    Name (_ADR, 0x00)  // _ADR: Address
                }

                Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                {
                    0x04, 
                    0x03
                })
                Method (_PSW, 1, NotSerialized)  // _PSW: Power State Wake
                {
                    If (Arg0)
                    {
                        \_SB.MEM.WED3 = 0x01
                    }
                    Else
                    {
                        \_SB.MEM.WED3 = 0x00
                    }
                }
            }

            Device (USB3)
            {
                Name (_ADR, 0x001D0002)  // _ADR: Address
                Device (HUB3)
                {
                    Name (_ADR, 0x00)  // _ADR: Address
                }
            }

            Device (USB4)
            {
                Name (_ADR, 0x001D0007)  // _ADR: Address
                Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                {
                    0x0D, 
                    0x03
                })
                Method (_PSW, 1, NotSerialized)  // _PSW: Power State Wake
                {
                    If (Arg0)
                    {
                        \_SB.MEM.WED3 = 0x01
                    }
                    Else
                    {
                        \_SB.MEM.WED3 = 0x00
                    }
                }

                Device (HUB0)
                {
                    Name (_ADR, 0x00)  // _ADR: Address
                    Device (POT1)
                    {
                        Name (_ADR, 0x01)  // _ADR: Address
                    }

                    Device (POT2)
                    {
                        Name (_ADR, 0x02)  // _ADR: Address
                    }
                }
            }

            Device (AMDM)
            {
                Name (_ADR, 0x001F0006)  // _ADR: Address
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                    While ((\_SB.MEM.ACLR == 0x00)) {}
                    \_SB.MEM.PMDM = 0x00
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                    \_SB.MEM.PMDM = 0x01
                }

                Method (_PSC, 0, NotSerialized)  // _PSC: Power State Current
                {
                    If (\_SB.MEM.PMDM)
                    {
                        Return (0x03)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                {
                    0x05, 
                    0x03
                })
                Method (_PSW, 1, NotSerialized)  // _PSW: Power State Wake
                {
                    If (Arg0)
                    {
                        \_SB.MEM.WED1 = 0x01
                    }
                    Else
                    {
                        \_SB.MEM.WED1 = 0x00
                    }
                }
            }

            Device (ASND)
            {
                Name (_ADR, 0x001F0005)  // _ADR: Address
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                    While ((\_SB.MEM.ACLR == 0x00)) {}
                    \_SB.MEM.PSND = 0x00
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                    \_SB.MEM.PSND = 0x01
                }

                Method (_PSC, 0, NotSerialized)  // _PSC: Power State Current
                {
                    If (\_SB.MEM.PSND)
                    {
                        Return (0x03)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }
            }

            Device (PCI1)
            {
                Name (_ADR, 0x00010000)  // _ADR: Address
                Name (_PRT, Package (0x01)  // _PRT: PCI Routing Table
                {
                    Package (0x04)
                    {
                        0xFFFF, 
                        0x00, 
                        \_SB.LNKG, 
                        0x00
                    }
                })
                Device (VGA)
                {
                    Name (_ADR, 0x00)  // _ADR: Address
                    Method (_PS0, 0, Serialized)  // _PS0: Power State 0
                    {
                        While ((\_SB.MEM.VGAR == 0x00)) {}
                        \_SB.MEM.SYU7 = 0x00
                        If ((\_SB.MEM.SYU6 == 0x01))
                        {
                            \_SB.MEM.SYU6 = 0x00
                            If ((\_SB.MEM.CTCA == 0x01))
                            {
                                \_SB.MEM.SYU7 = 0x01
                            }
                        }

                        \_SB.MEM.IESI = 0x01
                        \_SB.MEM.IEDI = 0x00
                        SMBR (0xFE00, 0x23, 0x0100, 0x00, 0xB2)
                        If ((\_SB.MEM.OEAX == 0x00))
                        {
                            \_SB.MEM.IESI = 0x01
                            \_SB.MEM.IEDI = 0x00
                            SMBR (0xFF00, 0x23, 0x0100, 0x00, 0xB2)
                            WPSX (0x0100, 0x01, 0x00, 0x00)
                            \_SB.MEM.VGAF = 0x00
                        }
                    }

                    Method (_PS3, 0, Serialized)  // _PS3: Power State 3
                    {
                        While ((\_SB.MEM.VGAR == 0x00)) {}
                        \_SB.MEM.IESI = 0x01
                        \_SB.MEM.IEDI = 0x00
                        SMBR (0xFE00, 0x23, 0x0100, 0x00, 0xB2)
                        If ((\_SB.MEM.OEAX == 0x00))
                        {
                            \_SB.MEM.IESI = 0x01
                            \_SB.MEM.IEDI = 0x00
                            SMBR (0xFF00, 0x23, 0x0100, 0x03, 0xB2)
                            WPSX (0x0100, 0x01, 0x00, 0x03)
                            \_SB.MEM.VGAF = 0x01
                        }
                    }

                    Method (_PSC, 0, NotSerialized)  // _PSC: Power State Current
                    {
                        While ((\_SB.MEM.VGAR == 0x00)) {}
                        \_SB.MEM.IESI = 0x01
                        \_SB.MEM.IEDI = 0x00
                        SMBR (0xFE00, 0x23, 0x0100, 0x00, 0xB2)
                        Return (\_SB.MEM.OEDX)
                    }

                    Method (_DOS, 1, NotSerialized)  // _DOS: Disable Output Switching
                    {
                        If ((Arg0 == 0x00))
                        {
                            \_SB.MEM.DCST = 0x01
                            \_SB.MEM.DOS2 = 0x00
                        }
                        ElseIf ((Arg0 == 0x01))
                        {
                            \_SB.MEM.DCST = 0x00
                            \_SB.MEM.DOS2 = 0x01
                        }
                        ElseIf ((Arg0 == 0x02))
                        {
                            \_SB.MEM.DCST = 0x01
                            \_SB.MEM.DOS2 = 0x01
                        }
                    }

                    Method (_DOD, 0, NotSerialized)  // _DOD: Display Output Devices
                    {
                        Name (BUFF, Package (0x02)
                        {
                            0x0110, 
                            0x0100
                        })
                        Return (BUFF) /* \_SB_.PCI0.PCI1.VGA_._DOD.BUFF */
                    }

                    Method (_ROM, 2, NotSerialized)  // _ROM: Read-Only Memory
                    {
                        Local0 = (Arg0 + 0x000C0000)
                        Local1 = (Arg1 << 0x03)
                        Name (BUFF, Buffer (Arg1) {})
                        Scope (\)
                        {
                            OperationRegion (VROM, SystemMemory, Local0, Local1)
                            Field (VROM, ByteAcc, NoLock, Preserve)
                            {
                                ROMI,   65536
                            }
                        }

                        BUFF = \ROMI
                        Return (BUFF) /* \_SB_.PCI0.PCI1.VGA_._ROM.BUFF */
                    }

                    Device (LCD)
                    {
                        Name (_ADR, 0x0110)  // _ADR: Address
                        Method (_DCS, 0, NotSerialized)  // _DCS: Display Current Status
                        {
                            If (\_SB.MEM.CTLA)
                            {
                                Return (0x0F)
                            }
                            Else
                            {
                                Return (0x0D)
                            }
                        }

                        Method (_DDC, 1, NotSerialized)  // _DDC: Display Data Current
                        {
                            If ((Arg0 == 0x01))
                            {
                                Local0 = 0x80
                            }
                            ElseIf ((Arg0 == 0x02))
                            {
                                Local0 = 0x0100
                            }
                            Else
                            {
                                Return (Zero)
                            }

                            \_SB.MEM.PRES = 0x00
                            Local1 = (Arg0 << 0x08)
                            Local1 |= 0x01
                            Name (BUFF, Buffer (Local0) {})
                            SMBR (0xFE00, 0x37, Local1, 0x000EF000, 0xB2)
                            Local1 &= 0xFF00
                            \_SB.MEM.OECX = 0x0100
                            While (\_SB.MEM.OECX)
                            {
                                SMBR (0xFE00, 0x37, Local1, 0x00, 0xB2)
                            }

                            Local0 = \_SB.MEM.FSDP
                            \_SB.MEM.FSDP = (Local0 | 0x22)
                            Local0 = (\_SB.MEM.FSDP - Local0)
                            \_SB.MEM.EDCK -= Local0
                            BUFF = \_SB.MEM.PRES
                            Return (BUFF) /* \_SB_.PCI0.PCI1.VGA_.LCD_._DDC.BUFF */
                        }

                        Method (_DGS, 0, NotSerialized)  // _DGS: Display Graphics State
                        {
                            If (\_SB.MEM.NXLA)
                            {
                                Return (One)
                            }
                            Else
                            {
                                Return (Zero)
                            }
                        }

                        Method (_DSS, 1, NotSerialized)  // _DSS: Device Set State
                        {
                            Local0 = Arg0
                            Local1 = (Local0 & 0x01)
                            If (Local1)
                            {
                                \_SB.MEM.NXLA = 0x01
                            }
                            Else
                            {
                                \_SB.MEM.NXLA = 0x00
                            }
                        }

                        Method (_BCL, 0, NotSerialized)  // _BCL: Brightness Control Levels
                        {
                            Name (BUFF, Package (0x0A)
                            {
                                0x64, 
                                0x0A, 
                                0x00, 
                                0x0F, 
                                0x1E, 
                                0x28, 
                                0x37, 
                                0x41, 
                                0x50, 
                                0x64
                            })
                            Return (BUFF) /* \_SB_.PCI0.PCI1.VGA_.LCD_._BCL.BUFF */
                        }

                        Method (_BCM, 1, NotSerialized)  // _BCM: Brightness Control Method
                        {
                            Local0 = (Arg0 * 0xFFFF)
                            Local0 /= 0x64
                            SMBR (0xFF00, 0x2A, Local0, 0x00, 0xB2)
                        }

                        Method (_PS0, 0, Serialized)  // _PS0: Power State 0
                        {
                            \_SB.MEM.LCDS = 0x00
                            If ((\_SB.MEM.RPPC == 0x01))
                            {
                                Notify (\_PR.CPU0, 0x80) // Status Change
                                \_SB.MEM.RPPC = 0x00
                            }

                            If ((\_SB.MEM.DNTF == 0x01))
                            {
                                \_SB.MEM.DNTF = 0x00
                                If ((\_SB.MEM.DOS2 == 0x00))
                                {
                                    If (\_SB.MEM.DCKU)
                                    {
                                        \_SB.MEM.NXDD = 0x01
                                    }

                                    If (((\_SB.MEM.CTLA != \_SB.MEM.NXLA) || ((\_SB.MEM.CTCA != \_SB.MEM.NXCA) || (
                                        (\_SB.MEM.CTDA != \_SB.MEM.NXDA) || (\_SB.MEM.CTTA != \_SB.MEM.NXTA)))))
                                    {
                                        Notify (\_SB.PCI0.PCI1.VGA, 0x80) // Status Change
                                    }
                                }
                            }

                            \_SB.MEM.DCKU = 0x00
                        }

                        Method (_PS3, 0, Serialized)  // _PS3: Power State 3
                        {
                            \_SB.MEM.LCDS = 0x03
                        }

                        Method (_PSC, 0, Serialized)  // _PSC: Power State Current
                        {
                            Return (\_SB.MEM.LCDS)
                        }
                    }

                    Device (CRT)
                    {
                        Name (_ADR, 0x0100)  // _ADR: Address
                        Method (_DCS, 0, NotSerialized)  // _DCS: Display Current Status
                        {
                            If (\_SB.MEM.CTCA)
                            {
                                Return (0x0F)
                            }
                            Else
                            {
                                Return (0x0D)
                            }
                        }

                        Method (_DDC, 1, NotSerialized)  // _DDC: Display Data Current
                        {
                            If ((\_SB.MEM.SYU7 == 0x01))
                            {
                                \_SB.MEM.SYU7 = 0x00
                                Return (Zero)
                            }

                            If ((Arg0 == 0x01))
                            {
                                Local0 = 0x80
                            }
                            ElseIf ((Arg0 == 0x02))
                            {
                                Local0 = 0x0100
                            }
                            Else
                            {
                                Return (Zero)
                            }

                            \_SB.MEM.PRES = 0x00
                            Local1 = (Arg0 << 0x08)
                            Local1 |= 0x02
                            Name (BUFF, Buffer (Local0) {})
                            SMBR (0xFE00, 0x37, Local1, 0x000EF000, 0xB2)
                            Local1 &= 0xFF00
                            \_SB.MEM.OECX = 0x0100
                            While (\_SB.MEM.OECX)
                            {
                                SMBR (0xFE00, 0x37, Local1, 0x00, 0xB2)
                            }

                            BUFF = \_SB.MEM.PRES
                            Return (BUFF) /* \_SB_.PCI0.PCI1.VGA_.CRT_._DDC.BUFF */
                        }

                        Method (_DGS, 0, NotSerialized)  // _DGS: Display Graphics State
                        {
                            If (\_SB.MEM.NXCA)
                            {
                                Return (One)
                            }
                            Else
                            {
                                Return (Zero)
                            }
                        }

                        Method (_DSS, 1, NotSerialized)  // _DSS: Device Set State
                        {
                            Local0 = Arg0
                            Local1 = (Local0 & 0x01)
                            If (Local1)
                            {
                                \_SB.MEM.NXCA = 0x01
                            }
                            Else
                            {
                                \_SB.MEM.NXCA = 0x00
                            }
                        }

                        Method (_PS0, 0, Serialized)  // _PS0: Power State 0
                        {
                            \_SB.MEM.CRTS = 0x00
                            If ((\_SB.MEM.RPPC == 0x01))
                            {
                                Notify (\_PR.CPU0, 0x80) // Status Change
                                \_SB.MEM.RPPC = 0x00
                            }
                        }

                        Method (_PS3, 0, Serialized)  // _PS3: Power State 3
                        {
                            \_SB.MEM.CRTS = 0x03
                        }

                        Method (_PSC, 0, Serialized)  // _PSC: Power State Current
                        {
                            Return (\_SB.MEM.CRTS)
                        }
                    }
                }
            }

            Method (_INI, 0, NotSerialized)  // _INI: Initialize
            {
                \_SB.MEM.BT1F = \_SB.MEM.BES1
                \_SB.MEM.DSPW = 0x00
                \_SB.MEM.VGAF = 0x00
                \_SB.MEM.VWE0 = 0x00
                \_SB.MEM.VWE1 = 0x00
                \_SB.MEM.PPSC = 0x00
                \_SB.MEM.SPSC = 0x00
                \_SB.MEM.GP91 = 0x00
                Local0 = 0x00
                If (CMPS (\_OS, "Microsoft Windows NT"))
                {
                    Local0 = 0x03
                    If (CondRefOf (\_OSI, Local1))
                    {
                        If (\_OSI ("Windows 2001"))
                        {
                            Local0 = 0x04
                        }
                    }
                }
                Else
                {
                    If (CMPS (\_OS, "Microsoft Windows"))
                    {
                        Local0 = 0x01
                    }

                    If (CMPS (\_OS, "Microsoft WindowsME:Millennium Edition"))
                    {
                        Local0 = 0x02
                    }
                }

                \_SB.MEM.OSID = Local0
                DIS (0x14)
                \_SB.MEM.OECX = 0x02000200
                If ((\_SB.MEM.OSID == 0x04))
                {
                    \_SB.MEM.OECX = 0x01040100
                }

                SMBR (0xFF00, 0x66, \_SB.MEM.OECX, 0x00, 0xB2)
                While ((\_SB.MEM.BTRE == 0x00)) {}
                SMBR (0xFF00, 0x1E, 0x01, 0x00, 0xB2)
                \_SB.MEM.PAR1 = 0x01
                \_SB.PCI0.FNC0.SYSR.TRP4 = 0x60
            }
        }

        Device (PWRB)
        {
            Name (_HID, EisaId ("PNP0C0C") /* Power Button Device */)  // _HID: Hardware ID
            Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
            {
                0x08, 
                0x04
            })
            Name (_STA, 0x0B)  // _STA: Status
        }

        Device (BT)
        {
            Name (_HID, "TOS6205")  // _HID: Hardware ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                If (\_SB.MEM.BTMD)
                {
                    Return (0x0F)
                }
                Else
                {
                    Return (0x00)
                }
            }

            Method (BTST, 0, NotSerialized)
            {
                \_SB.MEM.OESI = 0x00
                SMBR (0xFE00, 0x4D, 0x01, 0x7D00, 0xB2)
                Local0 = 0x01
                While (Local0)
                {
                    SMBR (0xFE00, 0x4D, 0x0101, 0x7D00, 0xB2)
                    Local2 = \_SB.MEM.OESI
                    Local1 = (\_SB.MEM.OEAX & 0xFF00)
                    If (Local1)
                    {
                        Local1 = (\_SB.MEM.OEAX & 0xFF)
                        If ((Local1 != 0x20))
                        {
                            Local2 = 0x00
                            Local0 = 0x00
                        }
                    }
                    Else
                    {
                        Local0 = 0x00
                    }
                }

                Local0 = (Local2 & 0x02)
                Local0 <<= 0x06
                Local1 = (Local2 & 0x04)
                Local1 <<= 0x04
                Local0 |= Local1
                Local3 = (Local2 & 0x10)
                Local3 >>= 0x04
                Local0 |= Local3
                Return (Local0)
            }

            Method (AUSB, 0, NotSerialized)
            {
                If (\_SB.MEM.BTMD)
                {
                    \_SB.MEM.IEDI = 0x00
                    \_SB.MEM.IESI = 0x03
                    SMBR (0xFF00, 0x4D, 0x01, 0x7C00, 0xB2)
                    Local0 = 0x01
                    While (Local0)
                    {
                        SMBR (0xFF00, 0x4D, 0x0101, 0x7C00, 0xB2)
                        Local1 = (\_SB.MEM.OEAX & 0xFF00)
                        If (Local1)
                        {
                            Local1 = (\_SB.MEM.OEAX & 0xFF)
                            If ((Local1 != 0x20))
                            {
                                Local0 = 0x00
                            }
                        }
                        Else
                        {
                            Local0 = 0x00
                        }
                    }
                }
            }

            Method (DUSB, 0, NotSerialized)
            {
                If (\_SB.MEM.BTMD)
                {
                    \_SB.MEM.IEDI = 0x00
                    \_SB.MEM.IESI = 0x04
                    SMBR (0xFF00, 0x4D, 0x01, 0x7C00, 0xB2)
                    Local0 = 0x01
                    While (Local0)
                    {
                        SMBR (0xFF00, 0x4D, 0x0101, 0x7C00, 0xB2)
                        Local1 = (\_SB.MEM.OEAX & 0xFF00)
                        If (Local1)
                        {
                            Local1 = (\_SB.MEM.OEAX & 0xFF)
                            If ((Local1 != 0x20))
                            {
                                Local0 = 0x00
                            }
                        }
                        Else
                        {
                            Local0 = 0x00
                        }
                    }
                }
            }

            Method (BTPO, 0, NotSerialized)
            {
                If (\_SB.MEM.BTMD)
                {
                    \_SB.MEM.IEDI = 0x00
                    \_SB.MEM.IESI = 0x01
                    SMBR (0xFF00, 0x4D, 0x01, 0x7C00, 0xB2)
                    Local0 = 0x01
                    While (Local0)
                    {
                        SMBR (0xFF00, 0x4D, 0x0101, 0x7C00, 0xB2)
                        Local1 = (\_SB.MEM.OEAX & 0xFF00)
                        If (Local1)
                        {
                            Local1 = (\_SB.MEM.OEAX & 0xFF)
                            If ((Local1 != 0x20))
                            {
                                Local0 = 0x00
                            }
                        }
                        Else
                        {
                            Local0 = 0x00
                        }
                    }
                }
            }

            Method (BTPF, 0, NotSerialized)
            {
                If (\_SB.MEM.BTMD)
                {
                    \_SB.MEM.IEDI = 0x00
                    \_SB.MEM.IESI = 0x02
                    SMBR (0xFF00, 0x4D, 0x01, 0x7C00, 0xB2)
                    Local0 = 0x01
                    While (Local0)
                    {
                        SMBR (0xFF00, 0x4D, 0x0101, 0x7C00, 0xB2)
                        Local1 = (\_SB.MEM.OEAX & 0xFF00)
                        If (Local1)
                        {
                            Local1 = (\_SB.MEM.OEAX & 0xFF)
                            If ((Local1 != 0x20))
                            {
                                Local0 = 0x00
                            }
                        }
                        Else
                        {
                            Local0 = 0x00
                        }
                    }
                }
            }
        }

        Device (LID)
        {
            Name (_HID, EisaId ("PNP0C0D") /* Lid Device */)  // _HID: Hardware ID
            Method (_LID, 0, NotSerialized)  // _LID: Lid Status
            {
                Return (\_SB.MEM.LIDS)
            }

            Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
            {
                0x0B, 
                0x04
            })
            Method (_PSW, 1, NotSerialized)  // _PSW: Power State Wake
            {
                If ((Arg0 == 0x00))
                {
                    \_SB.MEM.EWLD = 0x00
                }
                Else
                {
                    \_SB.MEM.EWLD = 0x01
                }
            }
        }

        Device (BAT1)
        {
            Name (_HID, EisaId ("PNP0C0A") /* Control Method Battery */)  // _HID: Hardware ID
            Name (_UID, 0x01)  // _UID: Unique ID
            Name (_PCL, Package (0x01)  // _PCL: Power Consumer List
            {
                \_SB
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                If (\_SB.MEM.BES1)
                {
                    Return (0x1F)
                }
                Else
                {
                    Return (0x0F)
                }
            }

            Method (_BIF, 0, NotSerialized)  // _BIF: Battery Information
            {
                Name (BUFF, Package (0x0D) {})
                BUFF [0x00] = 0x00
                Local2 = \_SB.MEM.BDV1
                Local0 = (\_SB.MEM.BDC1 * Local2)
                Divide (Local0, 0x03E8, Local1, Local0)
                BUFF [0x01] = Local0
                Local0 = (\_SB.MEM.BLF1 * Local2)
                Divide (Local0, 0x03E8, Local1, Local0)
                BUFF [0x02] = Local0
                BUFF [0x03] = \_SB.MEM.BTC1
                BUFF [0x04] = \_SB.MEM.BDV1
                Local0 = (\_SB.MEM.BCW1 * Local2)
                Divide (Local0, 0x03E8, Local1, Local0)
                BUFF [0x05] = Local0
                Local0 = (\_SB.MEM.BCL1 * Local2)
                Divide (Local0, 0x03E8, Local1, Local0)
                BUFF [0x06] = Local0
                Local0 = (\_SB.MEM.BG11 * Local2)
                Divide (Local0, 0x03E8, Local1, Local0)
                BUFF [0x07] = Local0
                Local0 = (\_SB.MEM.BG21 * Local2)
                Divide (Local0, 0x03E8, Local1, Local0)
                BUFF [0x08] = Local0
                BUFF [0x09] = \_SB.MEM.BMN1
                BUFF [0x0A] = \_SB.MEM.BSN1
                BUFF [0x0B] = \_SB.MEM.BTP1
                BUFF [0x0C] = \_SB.MEM.BOI1
                Return (BUFF) /* \_SB_.BAT1._BIF.BUFF */
            }

            Method (_BST, 0, NotSerialized)  // _BST: Battery Status
            {
                If (\_SB.MEM.BES2)
                {
                    Local0 = (\_SB.MEM.BST1 & 0x03)
                    Local1 = (\_SB.MEM.BST2 & 0x03)
                    If ((Local0 || Local1))
                    {
                        Local0 = (\_SB.MEM.BPR1 * \_SB.MEM.BDV1)
                        Divide (Local0, 0x07D0, Local1, Local0)
                    }
                    Else
                    {
                        Local0 = 0x00
                    }
                }
                ElseIf ((\_SB.MEM.BST1 && 0x03))
                {
                    Local0 = (\_SB.MEM.BPR1 * \_SB.MEM.BDV1)
                    Divide (Local0, 0x03E8, Local1, Local0)
                }
                Else
                {
                    Local0 = 0x00
                }

                Name (BUFF, Package (0x04) {})
                BUFF [0x00] = \_SB.MEM.BST1
                BUFF [0x01] = Local0
                Local0 = (\_SB.MEM.BRC1 * \_SB.MEM.BDV1)
                Divide (Local0, 0x03E8, Local1, Local0)
                BUFF [0x02] = Local0
                BUFF [0x03] = \_SB.MEM.BPV1
                Return (BUFF) /* \_SB_.BAT1._BST.BUFF */
            }

            Method (_BTP, 1, NotSerialized)  // _BTP: Battery Trip Point
            {
                \_SB.MEM.PAR1 = 0x01
                \_SB.MEM.PAR2 = Arg0
                \_SB.PCI0.FNC0.SYSR.TRP4 = 0x61
            }
        }

        Device (ADP1)
        {
            Name (_HID, "ACPI0003" /* Power Source Device */)  // _HID: Hardware ID
            Name (_PCL, Package (0x02)  // _PCL: Power Consumer List
            {
                \_SB, 
                \_SB.BAT1
            })
            Name (_STA, 0x0F)  // _STA: Status
            Method (_PSR, 0, NotSerialized)  // _PSR: Power Source
            {
                Return (\_SB.MEM.ACST)
            }
        }

        Device (VALZ)
        {
            Name (_HID, EisaId ("TOS6208"))  // _HID: Hardware ID
            Name (_DDN, "VALZeneral")  // _DDN: DOS Device Name
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                If (\_SB.MEM.SYU5)
                {
                    While ((\_SB.MEM.ACVA == 0x00)) {}
                    \_SB.MEM.SYU5 = 0x00
                }

                Return (0x0B)
            }

            Method (ENAB, 0, NotSerialized)
            {
                \_SB.MEM.VALF = 0x01
                SMBR (0xFF00, 0x16, 0x01, 0x00, 0xB2)
            }

            Method (INFO, 0, NotSerialized)
            {
                \_SB.MEM.OECX = 0x00
                SMBR (0xFE00, 0x16, 0x00, 0x00, 0xB2)
                If ((\_SB.MEM.OEAX != 0x00))
                {
                    \_SB.MEM.OECX = 0x00
                }

                Return (\_SB.MEM.OECX)
            }

            Method (GHCI, 6, Serialized)
            {
                CreateDWordField (Arg0, 0x00, REAX)
                CreateWordField (Arg1, 0x00, R_BX)
                Local0 = (REAX & 0xFF00)
                If ((Local0 == 0xFE00))
                {
                    If ((R_BX == 0xC000))
                    {
                        Return (G000 (Local0, R_BX, Arg2, Arg3, Arg4, Arg5))
                    }

                    If ((R_BX == 0xC800))
                    {
                        Return (G800 (Local0, R_BX, Arg2, Arg3, Arg4, Arg5))
                    }

                    If ((R_BX == 0xC801))
                    {
                        Return (G801 (Local0, R_BX, Arg2, Arg3, Arg4, Arg5))
                    }
                }

                If ((Local0 == 0xFF00))
                {
                    If ((R_BX == 0xC000))
                    {
                        Return (G000 (Local0, R_BX, Arg2, Arg3, Arg4, Arg5))
                    }

                    If ((R_BX == 0xC801))
                    {
                        Return (G801 (Local0, R_BX, Arg2, Arg3, Arg4, Arg5))
                    }
                }

                Return (GCH0 (Arg0, Arg1, Arg2, Arg3, Arg4, Arg5))
            }

            Method (GCH0, 6, NotSerialized)
            {
                \_SB.MEM.IESI = Arg4
                \_SB.MEM.IEDI = Arg5
                SMBR (Arg0, Arg1, Arg2, Arg3, 0xB2)
                Name (BUFF, Package (0x06) {})
                BUFF [0x00] = \_SB.MEM.OEAX
                BUFF [0x01] = \_SB.MEM.OEBX
                BUFF [0x02] = \_SB.MEM.OECX
                BUFF [0x03] = \_SB.MEM.OEDX
                BUFF [0x04] = \_SB.MEM.OESI
                BUFF [0x05] = \_SB.MEM.OEDI
                Return (BUFF) /* \_SB_.VALZ.GCH0.BUFF */
            }

            Method (G000, 6, NotSerialized)
            {
                Name (BUFF, Package (0x06) {})
                CreateDWordField (Arg2, 0x00, RECX)
                CreateDWordField (Arg3, 0x00, REDX)
                CreateDWordField (Arg4, 0x00, RESI)
                CreateDWordField (Arg5, 0x00, REDI)
                CreateByteField (Arg2, 0x00, R_CL)
                BUFF [0x00] = 0x00
                BUFF [0x01] = Arg1
                BUFF [0x02] = RECX /* \_SB_.VALZ.G000.RECX */
                BUFF [0x03] = REDX /* \_SB_.VALZ.G000.REDX */
                BUFF [0x04] = RESI /* \_SB_.VALZ.G000.RESI */
                BUFF [0x05] = REDI /* \_SB_.VALZ.G000.REDI */
                If (\_SB.MEM.GCVS)
                {
                    If ((Arg0 == 0xFE00))
                    {
                        If ((R_CL == 0x00))
                        {
                            Local0 = \_SB.MEM.TNVS
                            BUFF [0x02] = Local0
                        }
                        ElseIf (((R_CL >= 0x01) && (R_CL <= 0x04)))
                        {
                            Local0 = R_CL /* \_SB_.VALZ.G000.R_CL */
                            Local0 |= 0x3000
                            SMBR (0xFA00, Local0, 0x00, 0x00, 0xB2)
                            BUFF [0x02] = \_SB.MEM.OECX
                            BUFF [0x03] = \_SB.MEM.OEDX
                        }
                        ElseIf ((R_CL == 0x05))
                        {
                            BUFF [0x02] = 0x21
                        }
                        Else
                        {
                            BUFF [0x00] = 0x8300
                        }
                    }
                    Else
                    {
                        CreateWordField (Arg3, 0x00, R_DX)
                        If ((R_CL == 0x00))
                        {
                            If ((R_DX == 0x00))
                            {
                                \_SB.MEM.TNVS = 0x00
                            }
                            Else
                            {
                                \_SB.MEM.TNVS = 0x01
                            }
                        }
                        ElseIf ((R_CL == 0x01))
                        {
                            Local0 = R_CL /* \_SB_.VALZ.G000.R_CL */
                            Local0 |= 0x3080
                            SMBR (0xFA00, Local0, R_DX, 0x00, 0xB2)
                        }
                        ElseIf ((R_CL == 0x02))
                        {
                            FindSetRightBit (R_DX, Local0)
                            \_SB.MEM.NXDD = Local0
                            If ((\_SB.MEM.OSID < 0x03))
                            {
                                Local0 |= 0x0100
                                SMBR (0xFF00, 0x1C, Local0, 0x00, 0xB2)
                                Local0 = (\_SB.MEM.OEAX & 0xFF00)
                                If ((Local0 == 0x00))
                                {
                                    Local0 = 0x80
                                    While ((Local0 == 0x80))
                                    {
                                        SMBR (0xFE00, 0x1C, 0x00, 0x00, 0xB2)
                                        Local0 = (\_SB.MEM.OECX & 0x80)
                                    }

                                    If (\_SB.MEM.CTLA)
                                    {
                                        If ((\_SB.MEM.LCDS == 0x00))
                                        {
                                            SMBR (0xFF00, 0x02, 0x01, 0x00, 0xB2)
                                            \_SB.MEM.OEDX = 0x01
                                            While (\_SB.MEM.OEDX)
                                            {
                                                SMBR (0xFE00, 0x02, 0x00, 0x00, 0xB2)
                                            }
                                        }
                                    }
                                }
                            }
                            Else
                            {
                                Notify (\_SB.PCI0.PCI1.VGA, 0x80) // Status Change
                            }
                        }
                        Else
                        {
                            BUFF [0x00] = 0x8300
                        }
                    }
                }
                Else
                {
                    BUFF [0x00] = 0x8000
                }

                Return (BUFF) /* \_SB_.VALZ.G000.BUFF */
            }

            Method (G800, 6, NotSerialized)
            {
                Local0 = \_SB.MEM.OSPC
                Name (BUFF, Package (0x06) {})
                CreateDWordField (Arg3, 0x00, REDX)
                CreateDWordField (Arg4, 0x00, RESI)
                CreateDWordField (Arg5, 0x00, REDI)
                BUFF [0x00] = 0x00
                BUFF [0x01] = Arg1
                BUFF [0x02] = Local0
                BUFF [0x03] = REDX /* \_SB_.VALZ.G800.REDX */
                BUFF [0x04] = RESI /* \_SB_.VALZ.G800.RESI */
                BUFF [0x05] = REDI /* \_SB_.VALZ.G800.REDI */
                Return (BUFF) /* \_SB_.VALZ.G800.BUFF */
            }

            Method (G801, 6, NotSerialized)
            {
                CreateDWordField (Arg2, 0x00, RECX)
                CreateDWordField (Arg3, 0x00, REDX)
                CreateDWordField (Arg4, 0x00, RESI)
                CreateDWordField (Arg5, 0x00, REDI)
                Local0 = 0x8300
                Local1 = RECX /* \_SB_.VALZ.G801.RECX */
                If ((REDX == 0x01))
                {
                    Local0 = 0x00
                    If ((Arg0 == 0xFE00))
                    {
                        Local1 = \_SB.MEM.PULD
                        Local2 = \_SB.MEM.PULA
                        Local2 <<= 0x08
                        Local1 |= Local2
                    }
                    Else
                    {
                        Local2 = (Local1 & 0xFF)
                        Local3 = (Local1 >> 0x08)
                        \_SB.MEM.PULD = Local2
                        \_SB.MEM.PULA = Local3
                    }
                }

                If ((REDX == 0x02))
                {
                    Local0 = 0x00
                    If ((Arg0 == 0xFE00))
                    {
                        Local1 = \_SB.MEM.BCLD
                        Local2 = \_SB.MEM.BCLA
                        Local2 <<= 0x08
                        Local1 |= Local2
                    }
                    Else
                    {
                        Local2 = (Local1 & 0xFF)
                        Local3 = (Local1 >> 0x08)
                        \_SB.MEM.BCLD = Local2
                        \_SB.MEM.BCLA = Local3
                    }
                }

                Name (BUFF, Package (0x06) {})
                BUFF [0x00] = Local0
                BUFF [0x01] = Arg1
                BUFF [0x02] = Local1
                BUFF [0x03] = REDX /* \_SB_.VALZ.G801.REDX */
                BUFF [0x04] = RESI /* \_SB_.VALZ.G801.RESI */
                BUFF [0x05] = REDI /* \_SB_.VALZ.G801.REDI */
                Return (BUFF) /* \_SB_.VALZ.G801.BUFF */
            }

            Method (VCID, 0, NotSerialized)
            {
                \_SB.MEM.RDID = 0x00
                \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
                \_SB.MEM.DLID = \_SB.MEM.RDID
                If (\_SB.MEM.DLID)
                {
                    \_SB.MEM.DCKD = 0x01
                }

                Return (\_SB.MEM.RDID)
            }

            Method (VUID, 0, NotSerialized)
            {
                \_SB.MEM.RDSN = 0x00
                \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
                \_SB.MEM.DSRN = \_SB.MEM.RDSN
                Return (\_SB.MEM.RDSN)
            }

            Method (VDCK, 1, NotSerialized)
            {
                If (Arg0)
                {
                    \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
                    If (\_SB.MEM.PAR1)
                    {
                        \_SB.PCI0.FNC0.SYSR.TRP4 = 0x41
                        Reset (\_SB.PCI0.PCIB.DKSQ)
                        Wait (\_SB.PCI0.PCIB.DKSQ, 0x0BB8)
                        Notify (\_SB.PCI0.PCI1.VGA, 0x80) // Status Change
                        Return (One)
                    }

                    Return (Zero)
                }
                Else
                {
                    Return (One)
                }
            }

            Method (VEJ0, 1, NotSerialized)
            {
                If (\_SB.MEM.DCKD)
                {
                    \_SB.MEM.DCKD = 0x00
                    \_SB.PCI0.FNC0.SYSR.TRP4 = 0x40
                    Reset (\_SB.PCI0.PCIB.DKSQ)
                    Wait (\_SB.PCI0.PCIB.DKSQ, 0x1388)
                }

                \_SB.MEM.DLID = 0x00
                \_SB.MEM.DSRN = 0x00
                Notify (\_SB.VALZ, 0x83) // Device-Specific Change
                Notify (\_SB.PCI0.PCI1.VGA, 0x80) // Status Change
                Notify (\_PR.CPU0, 0x80) // Status Change
            }

            Method (DLSZ, 0, NotSerialized)
            {
                Return (0x04)
            }

            Method (DLIB, 0, NotSerialized)
            {
                Name (BUFF, Buffer (0x40)
                {
                    /* 0000 */  0x80, 0x00, 0x03, 0x0C, 0x01, 0xE9, 0x00, 0x00,  // ........
                    /* 0008 */  0x86, 0x80, 0xC4, 0x24, 0x00, 0x00, 0x00, 0x00,  // ...$....
                    /* 0010 */  0x80, 0x00, 0x03, 0x0C, 0x02, 0xE9, 0x00, 0x00,  // ........
                    /* 0018 */  0x86, 0x80, 0xC4, 0x24, 0x00, 0x00, 0x00, 0x00,  // ...$....
                    /* 0020 */  0x80, 0x20, 0x03, 0x0C, 0x03, 0xEF, 0x00, 0x00,  // . ......
                    /* 0028 */  0x86, 0x80, 0xCD, 0x24, 0x00, 0x00, 0x00, 0x00,  // ...$....
                    /* 0030 */  0x80, 0x20, 0x03, 0x0C, 0x04, 0xEF, 0x00, 0x00,  // . ......
                    /* 0038 */  0x86, 0x80, 0xCD, 0x24, 0x00, 0x00, 0x00, 0x00   // ...$....
                })
                Return (BUFF) /* \_SB_.VALZ.DLIB.BUFF */
            }

            Method (VNTF, 1, NotSerialized)
            {
                Local0 = (Arg0 & 0xFFFF)
                Local1 = (Arg0 >> 0x10)
                If ((Local1 == 0x01))
                {
                    Notify (\_PR.CPU0, Local0)
                }
            }

            Method (PLST, 0, NotSerialized)
            {
                Name (BUFF, Buffer (0x01)
                {
                     0x00                                             // .
                })
                CreateBitField (BUFF, 0x00, PNL0)
                CreateBitField (BUFF, 0x01, PNL1)
                PNL0 = \_SB.MEM.PNLS
                PNL1 = \_SB.MEM.LIDS
                Return (BUFF) /* \_SB_.VALZ.PLST.BUFF */
            }
        }
    }

    Scope (\_TZ)
    {
        PowerResource (PFAN, 0x00, 0x0000)
        {
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                SMBR (0xFA00, 0x2201, 0x00, 0x00, 0xB2)
                If (\_SB.MEM.OECX)
                {
                    Return (One)
                }
                Else
                {
                    Return (0x00)
                }
            }

            Method (_ON, 0, Serialized)  // _ON_: Power On
            {
                SMBR (0xFA00, 0x2200, 0xFF, 0x00, 0xB2)
            }

            Method (_OFF, 0, Serialized)  // _OFF: Power Off
            {
                SMBR (0xFA00, 0x2200, 0x00, 0x00, 0xB2)
            }
        }

        Device (FAN)
        {
            Name (_HID, EisaId ("PNP0C0B") /* Fan (Thermal Solution) */)  // _HID: Hardware ID
            Name (_PR0, Package (0x01)  // _PR0: Power Resources for D0
            {
                \_TZ.PFAN
            })
        }

        ThermalZone (THRM)
        {
            Method (_TMP, 0, NotSerialized)  // _TMP: Temperature
            {
                If ((\_SB.MEM.TMP1 <= 0x0B4C))
                {
                    \_SB.MEM.AST1 = 0x0B4C
                    Return (0x0B4C)
                }
                Else
                {
                    \_SB.MEM.AST1 = \_SB.MEM.TMP1
                    Return (\_SB.MEM.TMP1)
                }
            }

            Method (_AC0, 0, NotSerialized)  // _ACx: Active Cooling, x=0-9
            {
                Return (\_SB.MEM.AC01)
            }

            Method (_AC1, 0, NotSerialized)  // _ACx: Active Cooling, x=0-9
            {
                Return (\_SB.MEM.AC11)
            }

            Name (_AL0, Package (0x01)  // _ALx: Active List, x=0-9
            {
                \_TZ.FAN
            })
            Name (_AL1, Package (0x01)  // _ALx: Active List, x=0-9
            {
                \_TZ.FAN
            })
            Method (_PSV, 0, NotSerialized)  // _PSV: Passive Temperature
            {
                Return (\_SB.MEM.PSV1)
            }

            Name (_PSL, Package (0x01)  // _PSL: Passive List
            {
                \_PR.CPU0
            })
            Method (_CRT, 0, NotSerialized)  // _CRT: Critical Temperature
            {
                Return (\_SB.MEM.CRT1)
            }

            Name (_TC1, 0x09)  // _TC1: Thermal Constant 1
            Name (_TC2, 0x02)  // _TC2: Thermal Constant 2
            Name (_TSP, 0x0708)  // _TSP: Thermal Sampling Period
        }
    }

    Scope (\_GPE)
    {
        Method (_L03, 0, Serialized)  // _Lxx: Level-Triggered GPE, xx=0x00-0xFF
        {
            Notify (\_SB.PCI0.USB1, 0x02) // Device Wake
            \_SB.MEM.GP75 = 0x00
        }

        Method (_L04, 0, Serialized)  // _Lxx: Level-Triggered GPE, xx=0x00-0xFF
        {
            Notify (\_SB.PCI0.USB2, 0x02) // Device Wake
            \_SB.MEM.GP75 = 0x00
        }

        Method (_L05, 0, Serialized)  // _Lxx: Level-Triggered GPE, xx=0x00-0xFF
        {
            If (\_SB.MEM.GP73)
            {
                \_SB.MEM.GP73 = 0x00
                Notify (\_SB.PCI0.AMDM, 0x02) // Device Wake
            }
        }

        Method (_L0C, 0, Serialized)  // _Lxx: Level-Triggered GPE, xx=0x00-0xFF
        {
            Notify (\_SB.PCI0.USB3, 0x02) // Device Wake
            \_SB.MEM.GP75 = 0x00
        }

        Method (_L0B, 0, Serialized)  // _Lxx: Level-Triggered GPE, xx=0x00-0xFF
        {
            If ((\_SB.PCI0.PCIB.LAN.PLVI != 0xFFFF))
            {
                Local0 = (\_SB.PCI0.PCIB.LAN.PMES & 0x80)
                If ((Local0 == 0x80))
                {
                    Notify (\_SB.PCI0.PCIB.LAN, 0x02) // Device Wake
                }
            }

            While ((\_SB.MEM.GP74 || (\_SB.MEM.GP83 || (\_SB.MEM.WSF0 || \_SB.MEM.WSF1))))
            {
                If (\_SB.MEM.GP74)
                {
                    \_SB.MEM.GP74 = 0x00
                    Notify (\_SB.PCI0.FNC0.PCC0, 0x02) // Device Wake
                    Notify (\_SB.PCI0.PCIB.VIY0, 0x02) // Device Wake
                }

                If (\_SB.MEM.GP83)
                {
                    \_SB.MEM.GP83 = 0x00
                    Notify (\_SB.PCI0.PCIB.LAN, 0x02) // Device Wake
                }

                If (\_SB.MEM.WSF0)
                {
                    \_SB.MEM.WSF0 = 0x00
                    Notify (\_SB.PCI0.PCIB.MPC0, 0x02) // Device Wake
                }

                If (\_SB.MEM.WSF1)
                {
                    \_SB.MEM.WSF1 = 0x00
                    Notify (\_SB.PCI0.PCIB.MPC1, 0x02) // Device Wake
                }
            }
        }

        Method (_L0D, 0, Serialized)  // _Lxx: Level-Triggered GPE, xx=0x00-0xFF
        {
            If (\_SB.MEM.GP77)
            {
                \_SB.MEM.GP77 = 0x00
                Notify (\_SB.PCI0.USB4, 0x02) // Device Wake
            }
        }
    }

    Method (_PTS, 1, NotSerialized)  // _PTS: Prepare To Sleep
    {
        If (((\_SB.MEM.VWE0 || \_SB.MEM.VWE1) || \_SB.MEM.VWE2))
        {
            \_SB.MEM.WED2 = 0x01
        }
        Else
        {
            \_SB.MEM.WED2 = 0x00
        }

        \_SB.MEM.BCDD = \_SB.MEM.CTDD
        If (((Arg0 >= 0x01) && (Arg0 <= 0x04)))
        {
            \_SB.MEM.PAR1 = \_SB.MEM.EWLD
            \_SB.PCI0.FNC0.SYSR.TRP4 = 0x60
        }

        \_SB.MEM.ACBK = \_SB.MEM.ACST
        \_SB.MEM.RPPC = 0x01
    }

    Method (TRAP, 1, NotSerialized)
    {
        Debug = (Arg0 + 0x12340000)
    }

    Method (SMBR, 5, NotSerialized)
    {
        \_SB.MEM.IEAX = Arg0
        \_SB.MEM.IEBX = Arg1
        \_SB.MEM.IECX = Arg2
        \_SB.MEM.IEDX = Arg3
        \_SB.PCI0.FNC0.SYSR.TRP4 = Arg4
    }

    Method (STA, 1, NotSerialized)
    {
        \_SB.MEM.PAR1 = Arg0
        \_SB.MEM.PAR2 = 0x00
        \_SB.MEM.PAR3 = 0x00
        \_SB.MEM.PAR4 = 0x00
        \_SB.MEM.PAR5 = 0x00
        \_SB.MEM.PAR6 = 0x00
        \_SB.PCI0.FNC0.SYSR.TRP4 = 0x01
        Return (\_SB.MEM.PAR4)
    }

    Method (CRS, 1, NotSerialized)
    {
        \_SB.MEM.PAR1 = Arg0
        \_SB.MEM.PAR2 = 0x00
        \_SB.MEM.PAR3 = 0x00
        \_SB.MEM.PAR4 = 0x00
        \_SB.MEM.PAR5 = 0x00
        \_SB.MEM.PAR6 = 0x00
        \_SB.PCI0.FNC0.SYSR.TRP4 = 0x01
        If ((\_SB.MEM.PAR3 == 0x00))
        {
            Return (Buffer (0x02)
            {
                 0x79, 0x00                                       // y.
            })
        }

        Name (BUFF, Buffer (\_SB.MEM.PAR3) {})
        BUFF = \_SB.MEM.PRES
        Return (BUFF) /* \CRS_.BUFF */
    }

    Method (PRS, 1, NotSerialized)
    {
        \_SB.MEM.PAR1 = Arg0
        \_SB.MEM.PAR2 = 0x01
        \_SB.MEM.PAR3 = 0x00
        \_SB.MEM.PAR4 = 0x00
        \_SB.MEM.PAR5 = 0x00
        \_SB.MEM.PAR6 = 0x00
        \_SB.PCI0.FNC0.SYSR.TRP4 = 0x01
        If ((\_SB.MEM.PAR3 == 0x00))
        {
            Return (Buffer (0x02)
            {
                 0x79, 0x00                                       // y.
            })
        }

        Name (BUFF, Buffer (\_SB.MEM.PAR3) {})
        BUFF = \_SB.MEM.PRES
        Return (BUFF) /* \PRS_.BUFF */
    }

    Method (SRS, 2, NotSerialized)
    {
        \_SB.MEM.PAR1 = Arg0
        \_SB.MEM.PAR2 = 0x00
        \_SB.MEM.PAR3 = 0x00
        \_SB.MEM.PAR4 = 0x00
        \_SB.MEM.PAR5 = 0x00
        \_SB.MEM.PAR6 = 0x00
        \_SB.MEM.PRES = Arg1
        \_SB.PCI0.FNC0.SYSR.TRP4 = 0x02
    }

    Method (DIS, 1, NotSerialized)
    {
        \_SB.MEM.PAR1 = Arg0
        \_SB.MEM.PAR2 = 0x00
        \_SB.MEM.PAR3 = 0x00
        \_SB.MEM.PAR4 = 0x00
        \_SB.MEM.PAR5 = 0x00
        \_SB.MEM.PAR6 = 0x00
        \_SB.PCI0.FNC0.SYSR.TRP4 = 0x03
    }

    Method (PS0, 1, NotSerialized)
    {
        \_SB.MEM.IESI = 0x00
        SMBR (0xFE00, 0x23, Arg0, 0x00, 0xB2)
        If ((\_SB.MEM.OEAX == 0x00))
        {
            \_SB.MEM.IESI = 0x00
            SMBR (0xFF00, 0x23, Arg0, 0x00, 0xB2)
            WPSX (Arg0, 0x00, 0x00, 0x00)
        }
    }

    Method (PS3, 1, NotSerialized)
    {
        \_SB.MEM.IESI = 0x00
        SMBR (0xFE00, 0x23, Arg0, 0x00, 0xB2)
        If ((\_SB.MEM.OEAX == 0x00))
        {
            \_SB.MEM.IESI = 0x00
            SMBR (0xFF00, 0x23, Arg0, 0x03, 0xB2)
            WPSX (Arg0, 0x00, 0x00, 0x03)
        }
    }

    Method (WPSX, 4, NotSerialized)
    {
        \_SB.MEM.IESI = Arg1
        \_SB.MEM.IEDI = Arg2
        SMBR (0xFE00, 0x23, Arg0, 0x00, 0xB2)
        While ((\_SB.MEM.OECX != 0x00))
        {
            \_SB.MEM.IESI = Arg1
            \_SB.MEM.IEDI = Arg2
            SMBR (0xFE00, 0x23, Arg0, 0x00, 0xB2)
        }
    }

    Method (PSC, 1, NotSerialized)
    {
        \_SB.MEM.IESI = 0x00
        SMBR (0xFE00, 0x23, Arg0, 0x00, 0xB2)
        Return (\_SB.MEM.OEDX)
    }

    Method (CMPS, 2, NotSerialized)
    {
        If ((SizeOf (Arg0) == SizeOf (Arg1)))
        {
            Return (One)
        }
        Else
        {
            Return (Zero)
        }
    }

    Method (STAL, 1, NotSerialized)
    {
        If ((Arg0 == 0x80))
        {
            Return (0x09)
        }
        Else
        {
            Return (0x0B)
        }
    }

    Method (CRSL, 1, NotSerialized)
    {
        Name (IRQB, ResourceTemplate ()
        {
            IRQ (Level, ActiveLow, Shared, _Y01)
                {}
        })
        CreateWordField (IRQB, \CRSL._Y01._INT, INTX)  // _INT: Interrupts
        If ((Arg0 < 0x10))
        {
            Local0 = (Arg0 & 0x0F)
            INTX = (0x01 << Local0)
        }

        Return (IRQB) /* \CRSL.IRQB */
    }

    Mutex (MTEX, 0x00)
}

