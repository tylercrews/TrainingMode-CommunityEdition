    # To be inserted at 800d8b58
    # Just after determining to start a boost grab in Interrupt_Grab_FromDashAttack

    .include "../../Globals.s"
    .include "../../m-ex/Header.s"

    .set playerdata, 31
    .set player, 30

    backupall

    # CHECK IF ENABLED
    li r0, OSD.BoostGrab
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Exit

    # CHECK FOR FOLLOWER
    mr r3, playerdata
    branchl r12, 0x80005510
    cmpwi r3, 0x1
    beq Exit

    # DISPLAY OSD
    li r3, OSD.BoostGrab 
    lbz r4, 0xC(playerdata)
    load r5, MSGCOLOR_WHITE
    bl Text
    mflr r6
    lhz r7, TM_FramesinCurrentAS(playerdata)
    Message_Display

    b Exit

Text:
    blrl
    .string "Boost Grab\nFrame %d"

Exit:
    restoreall
    addi	r3, r30, 0
