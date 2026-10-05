    # To be inserted at 80040fa4
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    .set entity, 31
    .set playerdata, 31
    .set player, 30
    .set text, 29
    .set ComboCount, 28
    .set hitbool, 27

    .set MessageAreaLength, 0x100
    .set PerPlayerMessageStructLength, 0x28

    backup

    # Backup Combo Count
    lhz ComboCount, 0x14(r3)

    # CHECK IF ENABLED
    li r0, OSD.ComboCounter         # PowerShield ID
    # lwz r4, -0xdbc(rtoc) #get frame data toggle bits
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Exit

    # Get Playerdata
    mr r3, r27
    branchl r12, PlayerBlock_LoadMainCharDataOffset
    mr player, r3
    lwz playerdata, 0x2C(player)

CheckForFollower:
    mr r3, playerdata
    branchl r12, 0x80005510
    cmpwi r3, 0x1
    beq Exit

    # Check If Combo Count is 2 Hits or Higher
    cmpwi ComboCount, 2
    blt Exit

    # get damage dealt
    branchl r12, 0x80041300
    mr r8, r3
    mr r3, 13                   # id
    lbz r4, 0xC(playerdata)     # queue
    li r5, MSGCOLOR_WHITE
    bl Text
    mflr r6
    mr r7, ComboCount
    Message_Display

    # Store Per Frame Function to Monitor When Hitstun Runs Out
    lwz r3, 0x2094(playerdata)
    # Check If Valid Player GObj
    branchl r12, 0x80086960
    cmpwi r3, 0
    beq Exit
    # Store Function
    bl HitstunMonitor
    mflr r3
    lwz r4, 0x2094(playerdata)
    lwz r4, 0x2C(r4)
    stw r3, TM_AnimCallback(r4)

    b Exit

###################
## TEXT CONTENTS ##
###################

Text:
    blrl
    .string "Combo Count\n%d Hits / %d%"
    .align 2

##############################

HitstunMonitor:
    blrl

    backup

    # Get Pointers
    mr player, r3
    lwz playerdata, 0x2C(player)

    # Get Damage Source (Person Combo'ing Me)
    lwz r3, 0x1868(playerdata)
    # Ensure It's a Player
    branchl r12, 0x80086960
    cmpwi r3, 0x0
    beq HitstunMonitor_SelfDestruct
    lwz r28, 0x1868(playerdata)
    lwz r28, 0x2C(r28)              # Save For Later

    # Check If Combo Has Ended
    # Am I in Hitstun?
    lbz r0, 0x221C(playerdata)
    rlwinm. r0, r0, 31, 31, 31
    bne HitstunMonitor_ContinueCombo
    # Am I in Missfoot?
    lwz r3, 0x10(playerdata)
    cmpwi r3, 0xFB
    beq HitstunMonitor_ContinueCombo
    # Am I in Non-IASA Landing?
    cmpwi r3, 0x2A
    bne HitstunMonitor_SkipIASALandingCheck
    lfs f1, 0x0894(playerdata)
    lfs f0, 0x01F4(playerdata)
    fcmpo cr0, f1, f0
    blt HitstunMonitor_ContinueCombo

HitstunMonitor_SkipIASALandingCheck:
    # Am I in Missed Tech, Tech Roll, Slow Getup, Or Getup Attack
    lwz r0, 0x10(playerdata)
    cmpwi r0, 0xB7
    blt HitstunMonitor_EndCombo
    cmpwi r0, 0xC9
    bgt HitstunMonitor_CheckGrab
    b HitstunMonitor_ContinueCombo

HitstunMonitor_CheckGrab:
    cmpwi r0, 0xDF
    blt HitstunMonitor_EndCombo
    cmpwi r0, 0xE8
    bgt HitstunMonitor_CheckThrow
    b HitstunMonitor_ContinueCombo

HitstunMonitor_CheckThrow:
    cmpwi r0, 0xEF
    blt HitstunMonitor_EndCombo
    cmpwi r0, 0xF3
    bgt HitstunMonitor_CheckSpecialThrows
    b HitstunMonitor_ContinueCombo

HitstunMonitor_CheckSpecialThrows:
    cmpwi r0, 0x10A
    blt HitstunMonitor_EndCombo
    cmpwi r0, 0x130
    bgt HitstunMonitor_EndCombo
    b HitstunMonitor_ContinueCombo

HitstunMonitor_EndCombo:
    # Find MessageBox Pointer
    # Get Player's Message Struct in r29
    load r3, 0x804a1f58
    lwz r29, 0x4(r3)
    lbz r3, 0xC(r28)
    mulli r4, r3, PerPlayerMessageStructLength
    add r29, r29, r4
    # Init Search Loop
    li r27, 0

HitstunMonitor_SearchLoop:
    mulli r3, r27, 8
    add r3, r3, r29
    lhz r4, 0x6(r3)                 # Get OSD ID
    cmpwi r4, 13
    beq HitstunMonitor_EditOSD
    addi r27, r27, 1
    cmpwi r27, 4
    ble HitstunMonitor_SearchLoop

    # OSD Not Found, Self Destruct
    b HitstunMonitor_SelfDestruct

HitstunMonitor_EditOSD:
    mr r26, r3                      # Backup OSD Info
    # Set Time To Expire
    li r3, 60
    sth r3, 0x4(r26)
    # Get Green On Stack
    load r3, 0x8dff6eff
    stw r3, 0xF0(sp)
    # Change Subtext's Colors
    lwz r3, 0x0(r26)
    li r4, 1
    addi r5, sp, 0xF0
    branchl r12, Text_ChangeTextColor # Text_SetColor
    lwz r3, 0x0(r26)
    li r4, 2
    addi r5, sp, 0xF0
    branchl r12, Text_ChangeTextColor

HitstunMonitor_SelfDestruct:
    li r3, 0
    stw r3, TM_AnimCallback(playerdata)

HitstunMonitor_ContinueCombo:

HitstunMonitor_Exit:
    restore
    blr

##############################

Exit:
    restore
    lmw r27, 0x0024(sp)
