    # To be inserted at 800814ec
    .include "../../../../Globals.s"
    .include "../../../../m-ex/Header.s"

    .set playerdata, 31
    .set player, 30

##########################################################
## 804a1f5c -> 804a1fd4 = Static Stock Icon Text Struct ##
## Is 0x80 long and is zero'd at the start ##
## of every VS Match ##
## Store Text Info here ##
##########################################################

    backupall

    lwz playerdata, 0x2c(player)

    # CHECK IF ENABLED
    li r0, OSD.Ledge                # Ledge Codes ID
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

    # Check If Under 15 Frames
    lhz r3, 0x2416(playerdata)
    subi r3, r3, 1
    cmpwi r3, 15
    bgt Exit

SetColor:
    load r5, MSGCOLOR_RED
    bl TextVulnerable
    mflr r6

    lhz r3, 0x2416(playerdata)
    subi r3, r3, 1
    cmpwi r3, 0
    bgt EndSetColor

    load r5, MSGCOLOR_GREEN
    bl TextPerfect
    mflr r6
EndSetColor:

    li r3, 19                   # ID
    lbz r4, 0xC(playerdata)     # queue
    lhz r7, 0x2416(playerdata)
    subi r7, r7, 1
    Message_Display

    b Exit

###################
## TEXT CONTENTS ##
###################

TextVulnerable:
    blrl
    .string "Ledgestall\nVulnerable: %d"
    .align 2

TextPerfect:
    blrl
    .string "Ledgestall\nPerfect"
    .align 2

##############################

Exit:
    restoreall
    lwz r0, 0x0034(sp)
