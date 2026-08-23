/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20200925 (32-bit version)
 * Copyright (c) 2000 - 2020 Intel Corporation
 * 
 * Disassembly of dbgp.dat, Sat Aug 22 21:38:32 2026
 *
 * ACPI Data Table [DBGP]
 *
 * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue
 */

[000h 0000   4]                    Signature : "DBGP"    [Debug Port table]
[004h 0004   4]                 Table Length : 00000034
[008h 0008   1]                     Revision : 01
[009h 0009   1]                     Checksum : D2
[00Ah 0010   6]                       Oem ID : "TOSHIB"
[010h 0016   8]                 Oem Table ID : "750     "
[018h 0024   4]                 Oem Revision : 00970814
[01Ch 0028   4]              Asl Compiler ID : "TASM"
[020h 0032   4]        Asl Compiler Revision : 04010000

[024h 0036   1]               Interface Type : 00
[025h 0037   3]                     Reserved : 000000

[028h 0040  12]          Debug Port Register : [Generic Address Structure]
[028h 0040   1]                     Space ID : 01 [SystemIO]
[029h 0041   1]                    Bit Width : 08
[02Ah 0042   1]                   Bit Offset : 00
[02Bh 0043   1]         Encoded Access Width : 00 [Undefined/Legacy]
[02Ch 0044   8]                      Address : 00000000000001E0


Raw Table Data: Length 52 (0x34)

    0000: 44 42 47 50 34 00 00 00 01 D2 54 4F 53 48 49 42  // DBGP4.....TOSHIB
    0010: 37 35 30 20 20 20 20 20 14 08 97 00 54 41 53 4D  // 750     ....TASM
    0020: 00 00 01 04 00 00 00 00 01 08 00 00 E0 01 00 00  // ................
    0030: 00 00 00 00                                      // ....
