    # To be inserted at 80005508
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    .set REG_FighterData, 31
    .set player, 30
    .set REG_TextColor, 29

##########################################################
## 804a1f5c -> 804a1fd4 = Static Stock Icon Text Struct ##
## Is 0x80 long and is zero'd at the start ##
## of every VS Match ##
## Store Text Info here ##
##########################################################

    backupall

    mr player, r4
    lwz REG_FighterData, 0x2C(player)

#####################
## CHECK IF IASA'd ##
#####################

    # CHECK IF ENABLED
    li r0, OSD.ActOoS                           # wavedash ID
    # lwz r4, -0xdbc(rtoc) #get frame data toggle bits
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r5, 1
    slw r0, r5, r0
    and. r0, r0, r4
    beq Exit

CheckForIASA:
    cmpwi r3, 0x1
    bne Exit

CheckForFollower:
    mr r3, REG_FighterData
    branchl r12, 0x80005510
    cmpwi r3, 0x1
    beq Exit

CheckForGuardOff:
    lwz r3, 0x10(REG_FighterData)
    cmpwi r3, 0xB4
    beq Exit

ShieldWaitCheck:
    # CHECK IF 3RD MOST RECENT AS WAS SHIELD STUN
    lhz r3, TM_TwoASAgo(REG_FighterData)
    cmpwi r3, 0xB5
    beq CreateText

GuardOffCheck:
    # CHECK IF 4TH MOST RECENT AS WAS SHIELD STUN
    lhz r3, TM_ThreeASAgo(REG_FighterData)
    cmpwi r3, 0xB5
    beq CreateText
    b Exit

CreateText:
    # Change color to Green if frame perfect act oos
    lhz r3, TM_ShieldFrames(REG_FighterData)    # get shield stun frames left
    cmpwi r3, 0x0
    bgt WhiteText
    li REG_TextColor, MSGCOLOR_GREEN
    b PrintMessage

WhiteText:
    li REG_TextColor, MSGCOLOR_WHITE
    b PrintMessage

PrintMessage:
    li r3, 3                                    # Message Kind
    lbz r4, 0xC(REG_FighterData)                # Message Queue
    mr r5, REG_TextColor
    bl OoS_String
    mflr r6
    lhz r7, TM_ShieldFrames(REG_FighterData)    # get shield stun frames left
    addi r7, r7, 1
    Message_Display

    b Exit

###################
## TEXT CONTENTS ##
###################

OoS_String:
    blrl
    .string "Act OoShield\nFrame %d"
    .align 2

##############################

Exit:
    restoreall
    blr
