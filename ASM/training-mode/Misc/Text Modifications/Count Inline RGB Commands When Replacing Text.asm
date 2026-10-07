    # To be inserted at 803a7068
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    # Native SetText measures the old body through this iterator. Color runs
    # are four bytes (opcode + RGB), not an end-of-text marker.
    cmplwi r0, 0x0C
    bne Original
    li r4, 4
    branch r12, 0x803a7084
Original:
    cmplwi r0, 0x0A               # Original spacing-command comparison.
