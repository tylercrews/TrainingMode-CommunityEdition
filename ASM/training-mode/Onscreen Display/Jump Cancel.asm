    # To be inserted at 800cb698
    .include "../../Globals.s"

    .set playerdata, 31
    .set player, 30

    backup
    mr player, r31
    lwz playerdata, 0x2c(player)

    # Check For Not JumpSqaut
    lwz r3, 0x10(playerdata)
    cmpwi r3, 0x18
    beq Exit

    # CHECK IF ENABLED
    li r0, OSD.ActOoJumpSquat                     # OSD Menu ID
    # lwz r4, -0xdbc(rtoc) #get frame data toggle bits
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

SetColor:
    load r5, MSGCOLOR_RED
    lhz r3, 0x2408(playerdata)
    cmpwi r3, 0x1
    bne EndSetColor
    load r5, MSGCOLOR_GREEN
EndSetColor:

    li r3, 19                   # ID
    lbz r4, 0xC(playerdata)     # queue
    lhz r7, 0x2408(playerdata)
    bl Text
    mflr r6
    Message_Display

    b Exit

###################
## TEXT CONTENTS ##
###################

Text:
    blrl
    .string "Jump Cancel\nFrame %d"
    .align 2

##############################

Exit:
    restore
    lwz r0, 0x0024(sp)
