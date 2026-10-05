.include "ASM/Globals.s"
.text
.global TestOSDTag
TestOSDTag:
    rlwinm r3, r3, 0, 24, 31
    ori r3, r3, (20 << 8)
    oris r3, r3, (0x4000 | 1 | (1 << 3))
    blr
.global TestSettingsRead
TestSettingsRead:
    SettingsRead SettingsField_OSDMask, 4
    blr
.global TestSettingsWrite
TestSettingsWrite:
    SettingsToggle 3, 4
    blr
.global TestSettingsRow
TestSettingsRow:
    SettingsReadIndexed SettingsField_Row, 3, 3
    blr
.global TestSeedXER
TestSeedXER:
    lis r3, 0xa000
    mtxer r3
    blr
.global TestReadXER
TestReadXER:
    mfxer r3
    blr

# Deliberately clobber volatile FPRs after the real service returns.
.global TestClobberGet
TestClobberGet:
    mflr r0
    stw r0, 4(r1)
    stwu r1, -32(r1)
    bl Settings_Get
    lis r12, TestFloat@ha
    lfd f0, TestFloat@l(r12)
    .irp num,1,2,3,4,5,6,7,8,9,10,11,12,13
        fmr f\num, f0
    .endr
    lwz r0, 36(r1)
    addi r1, r1, 32
    mtlr r0
    blr
.section .rodata
.balign 8
TestFloat:
    .long 0x400921FB, 0x54442D18
