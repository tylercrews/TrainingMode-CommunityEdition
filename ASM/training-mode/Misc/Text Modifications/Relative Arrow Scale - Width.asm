    # To be inserted at 803a824c
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    lhz r0, 1(r23)              # Original first scale component.
    cmplwi r0, 0xFFFF
    bne Original
    lhz r0, 3(r23)
    cmplwi r0, 0xFFFF
    bne Original
    mflr r12
    bl FactorEnd
    .float 0.75
FactorEnd:
    mflr r11
    lfs f1, 0(r11)
    mtlr r12
    lfs f0, 0x80(r24)
    fmuls f0, f0, f1
    stfs f0, 0x80(r24)
    lfs f0, 0x84(r24)
    fmuls f0, f0, f1
    stfs f0, 0x84(r24)
    addi r23, r23, 4
    branch r12, 0x803a8288
Original:
    lhz r0, 1(r23)
