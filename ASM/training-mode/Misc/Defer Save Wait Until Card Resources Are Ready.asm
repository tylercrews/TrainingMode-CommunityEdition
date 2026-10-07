    # To be inserted at 8001cdb4
    .include "../../Globals.s"
    # The synchronous native drain must not spin on a deferred dirty flag.
    load r12, 0x80433318
    lwz r11, 0x18(r12)
    cmpwi r11, 0
    beq NotReady
    lwz r11, 0x5C(r12)
    cmpwi r11, 0
    beq NotReady
    load r12, 0x80432A68
    lwz r11, 0(r12)
    cmpwi r11, 0
    beq NotReady
    lwz r11, 4(r12)
    cmpwi r11, 0
    beq NotReady
    mflr r0
    b End
NotReady:
    blr
End:
