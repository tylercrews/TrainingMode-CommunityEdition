    # To be inserted at 8009ab78
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    .set playerdata, 31
    .set player, 30

    stw r0, 0x2064(r6)

    backupall

    mr playerdata, r6

    # CHECK IF ENABLED
    li r0, OSD.Ledge                # PowerShield ID
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Exit

CheckForFollower:
    mr r3, playerdata
    branchl r12, 0x80005510
    cmpwi r3, 0x1
    beq Exit

    # Check if Over 20 Frames
    lhz r3, 0x23f8(playerdata)
    cmpwi r3, 20
    bgt Exit

SetColor:
    load r5, MSGCOLOR_RED
    lhz r3, 0x23f8(playerdata)
    subi r3, r3, 1
    cmpwi r3, 0x1
    bne EndSetColor
    load r5, MSGCOLOR_GREEN
EndSetColor:

    li r3, 13                   # ID
    lbz r4, 0xC(playerdata)     # queue
    lhz r7, 0x23f8(playerdata)
    subi r7, r7, 1
    bl Text
    mflr r6
    Message_Display

    b Exit

###################
## TEXT CONTENTS ##
###################

Text:
    blrl
    .string "Frames in\nCliffwait: %d"
    .align 2

##############################

Exit:
    restoreall
