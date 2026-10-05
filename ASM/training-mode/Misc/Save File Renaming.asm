    # To be inserted at 8001c800
    .include "../../Globals.s"
    .include "../../m-ex/Header.s"
    .include "../../../build/tyro-identity.s"

    # Check for Tyro's stable disc/save identity (generated from version.h).
    lis r11, 0x8000
    lwz r11, 0x0(r11)
    load r12, TyroGameCode
    cmpw r11, r12
    beq ISO
    b Memcard

ISO:
    bl ISOSaveName
    mflr r5
    b exit

Memcard:
    bl MemcardSaveName
    mflr r5
    bl MemcardStringFormat
    mflr r4
    b exit

ISOSaveName:
    blrl
    TyroSaveCaption

MemcardSaveName:
    blrl
    .long 0x53757065
    .long 0x7220536D
    .long 0x61736820
    .long 0x42726F73
    .long 0x2E204D65
    .long 0x6C656520
    .long 0x20202020
    .long 0x20202020

MemcardSaveDesc:
    # blrl
    .long 0x4d6f6420
    .long 0x4c61756e
    .long 0x63686572
    .long 0x20283120
    .long 0x6f662032
    .long 0x29000000

MemcardStringFormat:
    blrl
    .long 0x25730000

exit:
    branchl r12, sprintf
