    # To be inserted at 80005504
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    .set entity, 31
    .set REG_FighterData, 31
    .set player, 30

    .set REG_TextColor, 29
    .set REG_Text, 28

##########################################################
## 804a1f5c -> 804a1fd4 = Static Stock Icon Text Struct ##
## Is 0x80 long and is zero'd at the start ##
## of every VS Match ##
## Store Text Info here ##
##########################################################

    backup

    mr player, r4
    lwz REG_FighterData, 0x2c(player)

    # Check For Interrupt
    cmpwi r3, 0x1
    bne Exit

    # CHECK IF ENABLED
    li r0, OSD.ActOoHitstun         # PowerShield ID
    # lwz r4, -0xdbc(rtoc) #get frame data toggle bits
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Exit

CheckForFollower:
    mr r3, REG_FighterData
    branchl r12, 0x80005510
    cmpwi r3, 0x1
    beq Exit

    # Check if frame 1
    lhz r3, TM_PostHitstunFrameCount(REG_FighterData)
    cmpwi r3, 0
    bgt WhiteText

GreenText:
    li REG_TextColor, MSGCOLOR_GREEN
    b PrintMessage

WhiteText:
    li REG_TextColor, MSGCOLOR_WHITE
    b PrintMessage

PrintMessage:
    li r3, 5                        # Message Kind
    lbz r4, 0xC(REG_FighterData)    # Message Queue
    mr r5, REG_TextColor
    bl ActOoHitstun_String
    mflr r6
    lhz r7, TM_PostHitstunFrameCount(REG_FighterData)
    addi r7, r7, 1
    Message_DisplayOSD 28, 1, 1, 0

    b Exit

###################
## TEXT CONTENTS ##
###################

ActOoHitstun_String:
    blrl
    .string "Act OoHitstun\nFrame %d"
    .align 2

##############################

Exit:
    restore
    blr
