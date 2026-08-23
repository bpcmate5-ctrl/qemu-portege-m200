/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20200925 (32-bit version)
 * Copyright (c) 2000 - 2020 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of ssdt.dat, Sat Aug 22 21:38:32 2026
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x00001640 (5696)
 *     Revision         0x01
 *     Checksum         0x37
 *     OEM ID           "TOSHIB"
 *     OEM Table ID     "A0009   "
 *     OEM Revision     0x20040107 (537133319)
 *     Compiler ID      "MSFT"
 *     Compiler Version 0x0100000E (16777230)
 */
DefinitionBlock ("", "SSDT", 1, "TOSHIB", "A0009   ", 0x20040107)
{
    External (_SB_.ADP1, UnknownObj)
    External (_SB_.BAT1, UnknownObj)
    External (_SB_.BT__, UnknownObj)
    External (_SB_.LID_, UnknownObj)
    External (_SB_.MEM_.ACBK, FieldUnitObj)
    External (_SB_.MEM_.ACST, FieldUnitObj)
    External (_SB_.MEM_.AST1, FieldUnitObj)
    External (_SB_.MEM_.BCDD, FieldUnitObj)
    External (_SB_.MEM_.BES1, FieldUnitObj)
    External (_SB_.MEM_.BES2, FieldUnitObj)
    External (_SB_.MEM_.BPFE, FieldUnitObj)
    External (_SB_.MEM_.BST1, FieldUnitObj)
    External (_SB_.MEM_.BT1F, FieldUnitObj)
    External (_SB_.MEM_.DCKD, FieldUnitObj)
    External (_SB_.MEM_.DCKF, FieldUnitObj)
    External (_SB_.MEM_.DCKU, FieldUnitObj)
    External (_SB_.MEM_.DLID, FieldUnitObj)
    External (_SB_.MEM_.DNTF, FieldUnitObj)
    External (_SB_.MEM_.DOS2, FieldUnitObj)
    External (_SB_.MEM_.DSPW, FieldUnitObj)
    External (_SB_.MEM_.DSRN, FieldUnitObj)
    External (_SB_.MEM_.GP50, FieldUnitObj)
    External (_SB_.MEM_.GP52, FieldUnitObj)
    External (_SB_.MEM_.GP53, FieldUnitObj)
    External (_SB_.MEM_.GP54, FieldUnitObj)
    External (_SB_.MEM_.GP56, FieldUnitObj)
    External (_SB_.MEM_.GP57, FieldUnitObj)
    External (_SB_.MEM_.GP60, FieldUnitObj)
    External (_SB_.MEM_.GP61, FieldUnitObj)
    External (_SB_.MEM_.GP62, FieldUnitObj)
    External (_SB_.MEM_.GP63, FieldUnitObj)
    External (_SB_.MEM_.GP64, FieldUnitObj)
    External (_SB_.MEM_.GP67, FieldUnitObj)
    External (_SB_.MEM_.GP70, FieldUnitObj)
    External (_SB_.MEM_.GP71, FieldUnitObj)
    External (_SB_.MEM_.GP91, FieldUnitObj)
    External (_SB_.MEM_.HKCD, FieldUnitObj)
    External (_SB_.MEM_.HPSU, FieldUnitObj)
    External (_SB_.MEM_.IEDI, FieldUnitObj)
    External (_SB_.MEM_.KBCR, FieldUnitObj)
    External (_SB_.MEM_.LANA, FieldUnitObj)
    External (_SB_.MEM_.OEAX, FieldUnitObj)
    External (_SB_.MEM_.OECX, FieldUnitObj)
    External (_SB_.MEM_.OSID, FieldUnitObj)
    External (_SB_.MEM_.PAR1, FieldUnitObj)
    External (_SB_.MEM_.PULA, FieldUnitObj)
    External (_SB_.MEM_.PULD, FieldUnitObj)
    External (_SB_.MEM_.RDID, FieldUnitObj)
    External (_SB_.MEM_.RDSN, FieldUnitObj)
    External (_SB_.MEM_.RPPC, FieldUnitObj)
    External (_SB_.MEM_.SBL4, FieldUnitObj)
    External (_SB_.MEM_.SYU5, FieldUnitObj)
    External (_SB_.MEM_.SYU6, FieldUnitObj)
    External (_SB_.MEM_.TMP1, FieldUnitObj)
    External (_SB_.MEM_.TNVS, FieldUnitObj)
    External (_SB_.MEM_.VALF, FieldUnitObj)
    External (_SB_.PCI0.FNC0, DeviceObj)
    External (_SB_.PCI0.FNC0.SYSR.TRP4, FieldUnitObj)
    External (_SB_.PCI0.PCI1.VGA_, UnknownObj)
    External (_SB_.PCI0.PCIB, DeviceObj)
    External (_SB_.PCI0.PCIB.BRGB, FieldUnitObj)
    External (_SB_.PCI0.PCIB.DKSQ, EventObj)
    External (_SB_.PCI0.PCIB.LAN_, UnknownObj)
    External (_SB_.PCI0.PCIB.SBSQ, EventObj)
    External (_SB_.PCI0.USB1, UnknownObj)
    External (_SB_.PCI0.USB2, UnknownObj)
    External (_SB_.PCI0.USB2.HUB2, DeviceObj)
    External (_SB_.PCI0.USB4.HUB0, DeviceObj)
    External (_SB_.PWRB, UnknownObj)
    External (_SB_.VALZ, UnknownObj)
    External (_TZ_.THRM, UnknownObj)
    External (CRS_, MethodObj)    // 1 Arguments
    External (DIS_, MethodObj)    // 1 Arguments
    External (PRS_, MethodObj)    // 1 Arguments
    External (SMBR, MethodObj)    // 5 Arguments
    External (SRS_, MethodObj)    // 2 Arguments
    External (STA_, MethodObj)    // 1 Arguments
    External (TRAP, MethodObj)    // 1 Arguments

    Scope (\_PR)
    {
        Processor (CPU0, 0x01, 0x0000D810, 0x06)
        {
            Name (_PCT, Package (0x02)  // _PCT: Performance Control
            {
                ResourceTemplate ()
                {
                    Register (FFixedHW, 
                        0x00,               // Bit Width
                        0x00,               // Bit Offset
                        0x0000000000000000, // Address
                        ,)
                }, 

                ResourceTemplate ()
                {
                    Register (FFixedHW, 
                        0x00,               // Bit Width
                        0x00,               // Bit Offset
                        0x0000000000000000, // Address
                        ,)
                }
            })
            Method (_PSS, 0, NotSerialized)  // _PSS: Performance Supported States
            {
                Return (TPSS) /* \_PR_.CPU0.TPSS */
            }

            Name (TPSS, Package (0x06)
            {
                Package (0x06)
                {
                    0x0640, 
                    0x5208, 
                    0x0A, 
                    0x0A, 
                    0x1028, 
                    0x1028
                }, 

                Package (0x06)
                {
                    0x0578, 
                    0x4844, 
                    0x0A, 
                    0x0A, 
                    0x0E24, 
                    0x0E24
                }, 

                Package (0x06)
                {
                    0x04B0, 
                    0x3C8C, 
                    0x0A, 
                    0x0A, 
                    0x0C20, 
                    0x0C20
                }, 

                Package (0x06)
                {
                    0x03E8, 
                    0x32C8, 
                    0x0A, 
                    0x0A, 
                    0x0A1B, 
                    0x0A1B
                }, 

                Package (0x06)
                {
                    0x0320, 
                    0x2710, 
                    0x0A, 
                    0x0A, 
                    0x0817, 
                    0x0817
                }, 

                Package (0x06)
                {
                    0x0258, 
                    0x1D4C, 
                    0x0A, 
                    0x0A, 
                    0x0612, 
                    0x0612
                }, 

                Package (0x06)
                {
                    0xFFFF, 
                    0xFFFF, 
                    0xFF, 
                    0xFF, 
                    0xFFFF, 
                    0xFFFF
                }, 

                Package (0x06)
                {
                    0xFFFF, 
                    0xFFFF, 
                    0xFF, 
                    0xFF, 
                    0xFFFF, 
                    0xFFFF
                }, 

                Package (0x06)
                {
                    0xFFFF, 
                    0xFFFF, 
                    0xFF, 
                    0xFF, 
                    0xFFFF, 
                    0xFFFF
                }, 

                Package (0x06)
                {
                    0xFFFF, 
                    0xFFFF, 
                    0xFF, 
                    0xFF, 
                    0xFFFF, 
                    0xFFFF
                }, 

                Package (0x06)
                {
                    0xFFFF, 
                    0xFFFF, 
                    0xFF, 
                    0xFF, 
                    0xFFFF, 
                    0xFFFF
                }, 

                Package (0x06)
                {
                    0xFFFF, 
                    0xFFFF, 
                    0xFF, 
                    0xFF, 
                    0xFFFF, 
                    0xFFFF
                }, 

                Package (0x06)
                {
                    0xFFFF, 
                    0xFFFF, 
                    0xFF, 
                    0xFF, 
                    0xFFFF, 
                    0xFFFF
                }, 

                Package (0x06)
                {
                    0xFFFF, 
                    0xFFFF, 
                    0xFF, 
                    0xFF, 
                    0xFFFF, 
                    0xFFFF
                }, 

                Package (0x06)
                {
                    0xFFFF, 
                    0xFFFF, 
                    0xFF, 
                    0xFF, 
                    0xFFFF, 
                    0xFFFF
                }, 

                Package (0x06)
                {
                    0xFFFF, 
                    0xFFFF, 
                    0xFF, 
                    0xFF, 
                    0xFFFF, 
                    0xFFFF
                }
            })
            Method (_PPC, 0, NotSerialized)  // _PPC: Performance Present Capabilities
            {
                SMBR (0xFC00, 0x3D, 0x00, 0x00, 0xB2)
                Local0 = (\_SB.MEM.OECX & 0x03)
                If ((Local0 == 0x00))
                {
                    If (\_SB.MEM.HPSU)
                    {
                        If (\_SB.MEM.ACST)
                        {
                            Return (\_SB.MEM.PULA) /* External reference */
                        }
                        Else
                        {
                            Return (\_SB.MEM.PULD) /* External reference */
                        }
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }
                Else
                {
                    Return (0x00)
                }
            }
        }
    }

    Scope (\_SB.PCI0.PCIB)
    {
        Device (DOCK)
        {
            Name (_HID, EisaId ("PNP0A05") /* Generic Container Device */)  // _HID: Hardware ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                SMBR (0x0136FA00, 0x05F3, 0x00, 0x00, 0xB2)
                Local0 = (\_SB.MEM.OEAX & 0xFF00)
                If ((((Local0 == 0x00) && (\_SB.MEM.OECX == 0x00)) || (Local0 != 
                    0x00)))
                {
                    \_SB.MEM.RDID = 0x00
                    \_SB.MEM.RDSN = 0x00
                    \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
                    \_SB.MEM.DLID = \_SB.MEM.RDID /* External reference */
                    \_SB.MEM.DSRN = \_SB.MEM.RDSN /* External reference */
                    If (\_SB.MEM.PAR1)
                    {
                        Return (0x00)
                    }
                    ElseIf (((0x2151F351 == \_SB.MEM.RDID) || (0x2051F351 == \_SB.MEM.RDID)))
                    {
                        \_SB.MEM.DCKD = 0x01
                        Return (0x0F)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }
                Else
                {
                    Return (0x00)
                }
            }

            Method (_BDN, 0, NotSerialized)  // _BDN: BIOS Dock Name
            {
                \_SB.MEM.RDID = 0x00
                \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
                Return (\_SB.MEM.RDID) /* External reference */
            }

            Method (_UID, 0, NotSerialized)  // _UID: Unique ID
            {
                \_SB.MEM.RDSN = 0x00
                \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
                Return (\_SB.MEM.RDSN) /* External reference */
            }

            Method (_DCK, 1, NotSerialized)  // _DCK: Dock Present
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

            Method (_EJ0, 1, NotSerialized)  // _EJx: Eject Device, x=0-9
            {
                If (\_SB.MEM.DCKD)
                {
                    If ((\_SB.MEM.BES1 || \_SB.MEM.BES2))
                    {
                        \_SB.MEM.DCKD = 0x00
                        \_SB.PCI0.FNC0.SYSR.TRP4 = 0x40
                        Reset (\_SB.PCI0.PCIB.DKSQ)
                        Wait (\_SB.PCI0.PCIB.DKSQ, 0x1388)
                    }
                }

                \_SB.MEM.DLID = 0x00
                \_SB.MEM.DSRN = 0x00
                Notify (\_SB.PCI0.PCIB.DOCK, 0x00) // Bus Check
                Notify (\_SB.PCI0.PCI1.VGA, 0x80) // Status Change
                Notify (\_PR.CPU0, 0x80) // Performance Capability Change
            }

            PowerResource (PDOC, 0x01, 0x0000)
            {
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    Return (\_SB.MEM.DSPW) /* External reference */
                }

                Method (_ON, 0, NotSerialized)  // _ON_: Power On
                {
                    \_SB.MEM.DSPW = 0x01
                }

                Method (_OFF, 0, NotSerialized)  // _OFF: Power Off
                {
                    \_SB.MEM.DSPW = 0x00
                }
            }

            Name (_PR0, Package (0x01)  // _PR0: Power Resources for D0
            {
                \_SB.PCI0.PCIB.DOCK.PDOC
            })
            Name (_PR1, Package (0x01)  // _PR1: Power Resources for D1
            {
                \_SB.PCI0.PCIB.DOCK.PDOC
            })
        }
    }

    Scope (\_SB.PCI0.USB2.HUB2)
    {
        Device (POT3)
        {
            Name (_ADR, 0x01)  // _ADR: Address
            Name (_EJD, "\\_SB_.PCI0.PCIB.DOCK")  // _EJD: Ejection Dependent Device
            Device (HB14)
            {
                Name (_ADR, 0x04)  // _ADR: Address
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
                    If ((0x2051F351 == \_SB.MEM.RDID))
                    {
                        Return (0x0F)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method (_EJ0, 1, NotSerialized)  // _EJx: Eject Device, x=0-9
                {
                    SMBR (0xFA00, 0x3100, 0x00, 0x00, 0xB2)
                    Reset (\_SB.PCI0.PCIB.SBSQ)
                    Wait (\_SB.PCI0.PCIB.SBSQ, 0x1388)
                }
            }

            Device (H_14)
            {
                Name (_ADR, 0x04)  // _ADR: Address
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
                    If ((0x2151F351 == \_SB.MEM.RDID))
                    {
                        Return (0x0F)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }
            }
        }

        Device (POT4)
        {
            Name (_ADR, 0x02)  // _ADR: Address
            Name (_EJD, "\\_SB_.PCI0.PCIB.DOCK")  // _EJD: Ejection Dependent Device
            Method (_EJ0, 1, NotSerialized)  // _EJx: Eject Device, x=0-9
            {
                SMBR (0xFA00, 0x3100, 0x00, 0x00, 0xB2)
                Reset (\_SB.PCI0.PCIB.SBSQ)
                Wait (\_SB.PCI0.PCIB.SBSQ, 0x1388)
            }
        }
    }

    Scope (\_SB.PCI0.USB4.HUB0)
    {
        Device (PT23)
        {
            Name (_ADR, 0x03)  // _ADR: Address
            Name (_EJD, "\\_SB_.PCI0.PCIB.DOCK")  // _EJD: Ejection Dependent Device
            Device (HB24)
            {
                Name (_ADR, 0x04)  // _ADR: Address
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
                    If ((0x2051F351 == \_SB.MEM.RDID))
                    {
                        Return (0x0F)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }

                Method (_EJ0, 1, NotSerialized)  // _EJx: Eject Device, x=0-9
                {
                    SMBR (0xFA00, 0x3100, 0x00, 0x00, 0xB2)
                    Reset (\_SB.PCI0.PCIB.SBSQ)
                    Wait (\_SB.PCI0.PCIB.SBSQ, 0x1388)
                }
            }

            Device (H_24)
            {
                Name (_ADR, 0x04)  // _ADR: Address
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
                    If ((0x2151F351 == \_SB.MEM.RDID))
                    {
                        Return (0x0F)
                    }
                    Else
                    {
                        Return (0x00)
                    }
                }
            }
        }

        Device (PT24)
        {
            Name (_ADR, 0x04)  // _ADR: Address
            Name (_EJD, "\\_SB_.PCI0.PCIB.DOCK")  // _EJD: Ejection Dependent Device
            Method (_EJ0, 1, NotSerialized)  // _EJx: Eject Device, x=0-9
            {
                SMBR (0xFA00, 0x3100, 0x00, 0x00, 0xB2)
                Reset (\_SB.PCI0.PCIB.SBSQ)
                Wait (\_SB.PCI0.PCIB.SBSQ, 0x1388)
            }
        }
    }

    Scope (\_GPE)
    {
        Method (_L08, 0, Serialized)  // _Lxx: Level-Triggered GPE, xx=0x00-0xFF
        {
            Local0 = \_SB.PCI0.PCIB.BRGB /* External reference */
            Local0 <<= 0x18
            Local0 |= 0x0020B10B
            \_SB.MEM.IEDI = 0xAF
            SMBR (0xFF00, Local0, 0x00, 0x00, 0xB2)
            While ((\_SB.MEM.GP50 || (\_SB.MEM.GP52 || (\_SB.MEM.GP53 || (\_SB.MEM.GP54 || (\_SB.MEM.GP56 || (
                \_SB.MEM.GP57 || (\_SB.MEM.GP60 || (\_SB.MEM.GP61 || (\_SB.MEM.GP62 || (\_SB.MEM.GP63 || (\_SB.MEM.GP64 || (\_SB.MEM.GP67 || 
                (\_SB.MEM.GP70 || (\_SB.MEM.GP71 || \_SB.MEM.BPFE)))))))))))))))
            {
                If (\_SB.MEM.GP50)
                {
                    \_SB.MEM.GP50 = 0x00
                    Notify (\_SB.ADP1, 0x80) // Status Change
                    Notify (\_PR.CPU0, 0x80) // Performance Capability Change
                }

                If (\_SB.MEM.GP52)
                {
                    \_SB.MEM.GP52 = 0x00
                    If ((\_SB.MEM.BES1 == \_SB.MEM.BT1F))
                    {
                        Notify (\_SB.BAT1, 0x80) // Status Change
                    }
                    Else
                    {
                        \_SB.MEM.BT1F = \_SB.MEM.BES1 /* External reference */
                        If (\_SB.MEM.BES1)
                        {
                            Notify (\_SB.BAT1, 0x00) // Bus Check
                        }
                        Else
                        {
                            Notify (\_SB.BAT1, 0x01) // Device Check
                        }
                    }
                }

                If (\_SB.MEM.GP53)
                {
                    \_SB.MEM.GP53 = 0x00
                    If ((\_SB.MEM.TMP1 != \_SB.MEM.AST1))
                    {
                        Notify (\_TZ.THRM, 0x80) // Status Change
                    }
                }

                If (\_SB.MEM.GP54)
                {
                    \_SB.MEM.GP54 = 0x00
                    If (\_SB.MEM.LANA)
                    {
                        \_SB.MEM.LANA = 0x00
                        Notify (\_SB.PCI0.PCIB.LAN, 0x01) // Device Check
                    }
                }

                If (\_SB.MEM.GP56)
                {
                    \_SB.MEM.GP56 = 0x00
                    Notify (\_SB.BAT1, 0x80) // Status Change
                }

                If (\_SB.MEM.GP57)
                {
                    \_SB.MEM.GP57 = 0x00
                    Notify (\_SB.VALZ, 0x85) // Device-Specific
                }

                If (\_SB.MEM.GP60)
                {
                    \_SB.MEM.GP60 = 0x00
                    SMBR (0x0136FA00, 0x05F3, 0x00, 0x00, 0xB2)
                    Local0 = (\_SB.MEM.OEAX & 0xFF00)
                    If ((((Local0 == 0x00) && (\_SB.MEM.OECX == 0x00)) || (Local0 != 
                        0x00)))
                    {
                        Notify (\_SB.PCI0.PCIB.DOCK, 0x00) // Bus Check
                    }
                    Else
                    {
                        Notify (\_SB.VALZ, 0x83) // Device-Specific Change
                    }

                    Notify (\_PR.CPU0, 0x80) // Performance Capability Change
                }

                If (\_SB.MEM.GP61)
                {
                    \_SB.MEM.GP61 = 0x00
                    If (\_SB.MEM.DCKD)
                    {
                        \_SB.MEM.DCKD = 0x00
                        SMBR (0x0136FA00, 0x05F3, 0x00, 0x00, 0xB2)
                        Local0 = (\_SB.MEM.OEAX & 0xFF00)
                        If ((((Local0 == 0x00) && (\_SB.MEM.OECX == 0x00)) || (Local0 != 
                            0x00)))
                        {
                            Notify (\_SB.PCI0.PCIB.DOCK, 0x01) // Device Check
                        }
                        Else
                        {
                            \_SB.MEM.DLID = 0x00
                            \_SB.MEM.DSRN = 0x00
                            Notify (\_SB.VALZ, 0x83) // Device-Specific Change
                            Notify (\_SB.PCI0.PCI1.VGA, 0x80) // Status Change
                            Notify (\_PR.CPU0, 0x80) // Performance Capability Change
                        }
                    }
                    Else
                    {
                        Signal (\_SB.PCI0.PCIB.DKSQ)
                    }
                }

                If (\_SB.MEM.GP62)
                {
                    \_SB.MEM.GP62 = 0x00
                    SMBR (0x0136FA00, 0x05F3, 0x00, 0x00, 0xB2)
                    Local0 = (\_SB.MEM.OEAX & 0xFF00)
                    If ((((Local0 == 0x00) && (\_SB.MEM.OECX == 0x00)) || (Local0 != 
                        0x00)))
                    {
                        Notify (\_SB.PCI0.PCIB.DOCK, 0x01) // Device Check
                    }
                    Else
                    {
                        Notify (\_SB.VALZ, 0x82) // Device-Specific Change
                    }
                }

                If (\_SB.MEM.GP63)
                {
                    \_SB.MEM.GP63 = 0x00
                    If ((\_SB.MEM.DCKF == 0x00))
                    {
                        \_SB.MEM.DCKF = 0x01
                        SMBR (0x0136FA00, 0x05F3, 0x00, 0x00, 0xB2)
                        Local0 = (\_SB.MEM.OEAX & 0xFF00)
                        If ((((Local0 == 0x00) && (\_SB.MEM.OECX == 0x00)) || (Local0 != 
                            0x00)))
                        {
                            Notify (\_SB.PCI0.PCIB.DOCK, 0x00) // Bus Check
                        }
                        Else
                        {
                            Notify (\_SB.VALZ, 0x81) // Information Change
                        }
                    }
                    Else
                    {
                        Signal (\_SB.PCI0.PCIB.DKSQ)
                        \_SB.MEM.DCKF = 0x00
                    }

                    Notify (\_PR.CPU0, 0x80) // Performance Capability Change
                }

                If (\_SB.MEM.GP64)
                {
                    \_SB.MEM.GP64 = 0x00
                    If (\_SB.MEM.SBL4)
                    {
                        \_SB.MEM.SBL4 = 0x00
                        Signal (\_SB.PCI0.PCIB.SBSQ)
                    }
                }

                If (\_SB.MEM.GP67)
                {
                    \_SB.MEM.GP67 = 0x00
                    Notify (\_SB.VALZ, 0x86) // Device-Specific
                }

                If (\_SB.MEM.GP70)
                {
                    \_SB.MEM.GP70 = 0x00
                    If (\_SB.MEM.VALF)
                    {
                        Notify (\_SB.VALZ, 0x80) // Status Change
                    }

                    If ((\_SB.MEM.HKCD == 0x3D))
                    {
                        TRAP (\_SB.MEM.HKCD)
                    }

                    If ((\_SB.MEM.DOS2 == 0x00))
                    {
                        If ((\_SB.MEM.HKCD == 0x3F))
                        {
                            If ((\_SB.MEM.TNVS == 0x00))
                            {
                                Notify (\_SB.PCI0.PCI1.VGA, 0x80) // Status Change
                            }
                        }
                    }
                }

                If (\_SB.MEM.GP71)
                {
                    \_SB.MEM.GP71 = 0x00
                    Notify (\_SB.LID, 0x80) // Status Change
                }

                If (\_SB.MEM.BPFE)
                {
                    \_SB.MEM.BPFE = 0x00
                    Notify (\_SB.BT, 0x90) // Device-Specific
                }
            }
        }
    }

    Scope (\_SB.PCI0.FNC0)
    {
        Device (PRT)
        {
            Name (_HID, EisaId ("PNP0401") /* ECP Parallel Port */)  // _HID: Hardware ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STA (0x10))
            }

            Name (_EJD, "\\_SB_.PCI0.PCIB.DOCK")  // _EJD: Ejection Dependent Device
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRS (0x10))
            }

            Method (_PRS, 0, NotSerialized)  // _PRS: Possible Resource Settings
            {
                Return (PRS (0x10))
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                SRS (0x10, Arg0)
            }

            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                DIS (0x10)
            }
        }

        Device (PRT1)
        {
            Name (_HID, EisaId ("PNP0400") /* Standard LPT Parallel Port */)  // _HID: Hardware ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STA (0x12))
            }

            Name (_EJD, "\\_SB_.PCI0.PCIB.DOCK")  // _EJD: Ejection Dependent Device
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (CRS (0x12))
            }

            Method (_PRS, 0, NotSerialized)  // _PRS: Possible Resource Settings
            {
                Return (PRS (0x12))
            }

            Method (_SRS, 1, NotSerialized)  // _SRS: Set Resource Settings
            {
                SRS (0x12, Arg0)
            }

            Method (_DIS, 0, NotSerialized)  // _DIS: Disable Device
            {
                DIS (0x12)
            }
        }
    }

    Method (_WAK, 1, NotSerialized)  // _WAK: Wake
    {
        Notify (\_SB.PCI0.USB1, 0x00) // Bus Check
        \_SB.MEM.RDID = 0x00
        \_SB.MEM.RDSN = 0x00
        \_SB.PCI0.FNC0.SYSR.TRP4 = 0x05
        If ((\_SB.MEM.PAR1 == 0x00))
        {
            If (((0x2151F351 == \_SB.MEM.RDID) || (0x2051F351 == \_SB.MEM.RDID)))
            {
                Notify (\_SB.PCI0.USB2, 0x00) // Bus Check
                If (((\_SB.MEM.RDID != \_SB.MEM.DLID) || (\_SB.MEM.RDSN != \_SB.MEM.DSRN)))
                {
                    SMBR (0x0136FA00, 0x05F3, 0x00, 0x00, 0xB2)
                    Local0 = (\_SB.MEM.OEAX & 0xFF00)
                    If ((((Local0 == 0x00) && (\_SB.MEM.OECX == 0x00)) || (Local0 != 
                        0x00)))
                    {
                        Notify (\_SB.PCI0.PCIB.DOCK, 0x00) // Bus Check
                    }
                    Else
                    {
                        Notify (\_SB.VALZ, 0x81) // Information Change
                    }
                }
            }
        }
        ElseIf (\_SB.MEM.DLID)
        {
            \_SB.MEM.DCKU = 0x01
            \_SB.MEM.DCKD = 0x00
            SMBR (0x0136FA00, 0x05F3, 0x00, 0x00, 0xB2)
            Local0 = (\_SB.MEM.OEAX & 0xFF00)
            If ((((Local0 == 0x00) && (\_SB.MEM.OECX == 0x00)) || (Local0 != 
                0x00)))
            {
                Notify (\_SB.PCI0.PCIB.DOCK, 0x01) // Device Check
            }
            Else
            {
                Notify (\_SB.VALZ, 0x83) // Device-Specific Change
            }
        }

        DIS (0x14)
        \_SB.MEM.OECX = 0x02000200
        If ((\_SB.MEM.OSID == 0x04))
        {
            \_SB.MEM.OECX = 0x01040100
        }

        SMBR (0xFF00, 0x66, \_SB.MEM.OECX, 0x00, 0xB2)
        SMBR (0xFF00, 0x1E, 0x01, 0x00, 0xB2)
        If ((\_SB.MEM.OSID <= 0x03))
        {
            While ((\_SB.MEM.KBCR == 0x00)) {}
        }

        \_SB.MEM.SYU6 = 0x00
        If ((Arg0 == 0x04))
        {
            Local0 = \_SB.MEM.BCDD /* External reference */
            If (((Local0 == 0x01) || (Local0 == 0x04)))
            {
                \_SB.MEM.SYU6 = 0x01
            }
        }

        If (((Arg0 == 0x03) || (Arg0 == 0x04)))
        {
            \_SB.MEM.DNTF = 0x01
        }

        \_SB.MEM.SYU5 = 0x01
        \_SB.MEM.PAR1 = 0x01
        \_SB.PCI0.FNC0.SYSR.TRP4 = 0x60
        If ((\_SB.MEM.ACST != \_SB.MEM.ACBK))
        {
            If ((\_SB.MEM.RPPC != 0x01))
            {
                Notify (\_PR.CPU0, 0x80) // Performance Capability Change
            }
        }

        Name (BUFF, Package (0x02)
        {
            0x00, 
            0x01
        })
        If ((\_SB.MEM.ACST == 0x00))
        {
            Local0 = (\_SB.MEM.BST1 & 0x04)
            If ((Local0 == 0x04))
            {
                BUFF [0x00] = 0x01
            }
        }

        If (\_SB.MEM.GP91)
        {
            \_SB.MEM.GP91 = 0x00
            If ((Arg0 == 0x04))
            {
                Notify (\_SB.PWRB, 0x02) // Device Wake
            }
        }

        Return (BUFF) /* \_WAK.BUFF */
    }
}

