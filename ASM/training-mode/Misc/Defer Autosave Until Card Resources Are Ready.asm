    # To be inserted at 8001cc84
    .include "../../Globals.s"

    # Native global polling also reaches this routine, not just C menu calls.
    # Do not consume xC/dirty while match scenes have unloaded icon/work data.
    load r12, 0x80433318
    lwz r11, 0x18(r12)          # lbCardGame archive enabled
    cmpwi r11, 0
    beq NotReady
    lwz r11, 0x5C(r12)          # Icon table dereferenced by 0x8001C868
    cmpwi r11, 0
    beq NotReady
    load r12, 0x80432A68
    lwz r11, 0(r12)             # Work area required by lbcardnew.c
    cmpwi r11, 0
    beq NotReady
    lwz r11, 4(r12)             # CARD transfer buffer
    cmpwi r11, 0
    beq NotReady
    mflr r0                    # Original first instruction; continue native state machine
    b End
NotReady:
    blr                        # Leave pending native dirty state untouched
End:
