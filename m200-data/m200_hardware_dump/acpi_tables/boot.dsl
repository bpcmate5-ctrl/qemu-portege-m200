/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20200925 (32-bit version)
 * Copyright (c) 2000 - 2020 Intel Corporation
 * 
 * Disassembly of boot.dat, Sat Aug 22 21:38:32 2026
 *
 * ACPI Data Table [BOOT]
 *
 * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue
 */

[000h 0000   4]                    Signature : "BOOT"    [Simple Boot Flag Table]
[004h 0004   4]                 Table Length : 00000028
[008h 0008   1]                     Revision : 01
[009h 0009   1]                     Checksum : 35
[00Ah 0010   6]                       Oem ID : "TOSHIB"
[010h 0016   8]                 Oem Table ID : "750     "
[018h 0024   4]                 Oem Revision : 00970814
[01Ch 0028   4]              Asl Compiler ID : "TASM"
[020h 0032   4]        Asl Compiler Revision : 04010000

[024h 0036   1]          Boot Register Index : 7C
[025h 0037   3]                     Reserved : 000000

Raw Table Data: Length 40 (0x28)

    0000: 42 4F 4F 54 28 00 00 00 01 35 54 4F 53 48 49 42  // BOOT(....5TOSHIB
    0010: 37 35 30 20 20 20 20 20 14 08 97 00 54 41 53 4D  // 750     ....TASM
    0020: 00 00 01 04 7C 00 00 00                          // ....|...
