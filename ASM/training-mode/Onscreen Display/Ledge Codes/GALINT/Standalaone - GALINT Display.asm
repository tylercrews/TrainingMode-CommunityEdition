    # To be inserted at 80005514
    .include "../../../../Globals.s"
    .include "../../../../m-ex/Header.s"

    .set playerdata, 31
    .set player, 30
    .set REG_isAerialInterrupt, 28
    .set REG_GALINT, 27

    backupall

    mr player, r3
    lwz playerdata, 0x2c(player)
    mr REG_isAerialInterrupt, r4

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

    # Check if Over 20 Frames past GALINT
    lhz r3, TM_TangibleFrameCount(playerdata)
    cmpwi r3, 20
    bgt Exit

    # Get GALINT Frames (Ledge Intang - Landing Lag)
    lwz REG_GALINT, 0x1990(playerdata)
    cmpwi REG_isAerialInterrupt, 0
    beq SkipAI
    lfs f1, 0x1F4(playerdata)
    fctiwz f1, f1
    stfd f1, 0x80(sp)
    lwz r3, 0x84(sp)
    sub REG_GALINT, REG_GALINT, r3
SkipAI:

SetColor:
    load r5, MSGCOLOR_RED
    lhz r7, TM_TangibleFrameCount(playerdata)
    neg r7, r7
    cmpwi REG_GALINT, 0x0
    ble EndSetColor
    load r5, MSGCOLOR_GREEN
    mr r7, REG_GALINT
EndSetColor:

    li r3, 19                   # ID
    lbz r4, 0xC(playerdata)     # queue
    bl Text
    mflr r6
    Message_DisplayOSD 26, 0, 1, 0

    b Exit

###################
## TEXT CONTENTS ##
###################

Text:
    blrl
    .string "GALINT\nFrames: %d"
    .align 2

##############################

Exit:
    restoreall
    blr
