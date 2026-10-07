    # To be inserted at 803a8db8
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    lhz r0, 1(r30)              # Native scale stack was pushed before this hook.
    cmplwi r0, 0xFFFF
    bne Original
    lhz r0, 3(r30)
    cmplwi r0, 0xFFFF
    bne Original
    mflr r12
    bl FactorEnd
    .float 0.75
FactorEnd:
    mflr r11
    lfs f1, 0(r11)
    mtlr r12
    lfs f0, 0x80(r31)
    fmuls f0, f0, f1
    stfs f0, 0x80(r31)
    lfs f0, 0x84(r31)
    fmuls f0, f0, f1
    stfs f0, 0x84(r31)
    addi r30, r30, 4
    branch r12, 0x803a937c
Original:
    lhz r0, 1(r30)
