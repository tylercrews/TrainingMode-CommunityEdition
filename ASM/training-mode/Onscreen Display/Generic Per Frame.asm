    # To be inserted at 8006b7f4
    # Runs once per frame, use for OSDs that don't require a specific injection point

    .include "../../Globals.s"
    .include "../../m-ex/Header.s"

    .set playerdata, 31
    .set player, 30

    backupall
    SettingsBackup
    mr r3, player
    rtocbl r12, TM_ShineBeforeIASA
    SettingsRestore
    
OSD_ActOoWait:
    # CHECK IF ENABLED
    li r0, OSD.ActOoWait                        # PowerShield ID
    # lwz r4, -0xdbc(rtoc) #get frame data toggle bits
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq ActOoWait_End

    # Act Oo Wait
    li r4, 0
ActOoWait_SearchForWait:
    # r3 = data->TM.prev_state[r4]
    mulli r3, r4, 2
    add r3, r3, playerdata
    lhz r3, TM_PrevASStart(r3)
    
    cmpwi r3, ASID_Wait
    beq ActOoWait_FoundWait
    cmpwi r3, ASID_Landing
    beq ActOoWait_FoundWait
    cmpwi r3, ASID_LandingFallSpecial
    beq ActOoWait_FoundWait
    cmpwi r3, ASID_LandingAirN
    blt ActOoWait_CheckIntermediate
    cmpwi r3, ASID_LandingAirLw
    ble ActOoWait_FoundWait
ActOoWait_CheckIntermediate:
    
    # If the player walks/turns as an intermediate state,
    # skip this intermediate state and show the OSD for the next state.
    cmpwi r3, ASID_WalkSlow
    beq ActOoWait_NextPrevState
    cmpwi r3, ASID_WalkMiddle
    beq ActOoWait_NextPrevState
    cmpwi r3, ASID_WalkFast
    beq ActOoWait_NextPrevState
    cmpwi r3, ASID_Turn
    beq ActOoWait_NextPrevState
    cmpwi r3, ASID_Squat
    beq ActOoWait_NextPrevState
    b ActOoWait_End
    
ActOoWait_NextPrevState:
    addi r4, r4, 1
    cmpwi r4, TM_PrevASSlots
    beq ActOoWait_End
    b ActOoWait_SearchForWait
    
ActOoWait_FoundWait:
    # Make Sure Player Didn't Buffer Crouch, Shield, Walk, or Turn
    lwz r3, 0x10(playerdata)
    cmpwi r3, ASID_Wait
    beq ActOoWait_End
    cmpwi r3, ASID_WalkSlow
    beq ActOoWait_End
    cmpwi r3, ASID_WalkMiddle
    beq ActOoWait_End
    cmpwi r3, ASID_WalkFast
    beq ActOoWait_End
    cmpwi r3, ASID_Squat
    beq ActOoWait_End
    cmpwi r3, ASID_GuardOn
    beq ActOoWait_End
    cmpwi r3, ASID_Turn
    beq ActOoWait_End
    cmpwi r3, ASID_SquatWait
    beq ActOoWait_End

    # Display OSD if enabled
    mr r3, player
    branchl r12, 0x8000551c
ActOoWait_End:

OSD_FighterSpecificTech:
    li r0, OSD.FighterSpecificTech    # OSD ID
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq FighterSpecificTech_End

    # Check Fighter
    lwz r3, 0x4(playerdata)  # load fighter Internal ID
    cmpwi r3, Fox.Int
    beq FoxFalco
    cmpwi r3, Falco.Int
    beq FoxFalco
    cmpwi r3, Yoshi.Int
    beq Yoshi

    # Check If Anyone Else
    b FighterSpecificTech_End

# /////////////////////////////////////////////////////////////////////////////

FoxFalco:
    lwz r3, 0x10(playerdata)  # load action state ID
    cmpwi r3, 0x15B                 # Ground Side B Start
    beq FoxFalco_SideBStart
    cmpwi r3, 0x15E                 # Air Side B Start
    beq FoxFalco_SideBStart

    cmpwi r3, 0x15C                 # Ground Side B
    beq FoxFalco_SideB
    cmpwi r3, 0x15F                 # Air Side B
    beq FoxFalco_SideB

    cmpwi r3, 0x15D                 # Ground Side B End
    beq FoxFalco_SideBEnd
    cmpwi r3, 0x160                 # Air Side B End
    beq FoxFalco_SideBEnd
    
    cmpwi r3, 0x16D                 # Ground Side B End
    beq FoxFalco_ShineAirStartup

    b FighterSpecificTech_End

# --------

FoxFalco_SideBStart:
    # Check If Pressed B
    lwz r3, 0x668(playerdata)
    rlwinm. r3, r3, 0, 22, 22
    beq FighterSpecificTech_End

    # Get Frames Early
    lwz r7, 0x590(playerdata)       # get anim data
    lfs f1, 0x008(r7)               # get anim length (float)
    fctiwz f1, f1
    stfd f1, 0xF0(sp)
    lwz r7, 0xF4(sp)
    lhz r3, 0x23F8(playerdata)
    sub r7, r7, r3
    subi r7, r7, 0x1

    li r3, OSD.FighterSpecificTech  # ID
    lbz r4, 0xC(playerdata)         # queue
    load r5, MSGCOLOR_RED
    bl FoxFalco_ShortenEarlyPressText
    mflr r6
    Message_DisplayOSD 8, 0, 1, 0

    b FighterSpecificTech_End

# --------

FoxFalco_SideB:
    # Check If Pressed B
    lwz r3, 0x668(playerdata)
    rlwinm. r3, r3, 0, 22, 22
    beq FighterSpecificTech_End

    # Get Press Frame
    lfs f1, 0x894(playerdata)
    fctiwz f1, f1
    stfd f1, 0xF0(sp)
    lwz r3, 0xF4(sp)
    addi r7, r3, 0x1

    li r3, OSD.FighterSpecificTech  # ID
    lbz r4, 0xC(playerdata)         # queue
    load r5, MSGCOLOR_GREEN
    bl FoxFalco_ShortenTypeText
    mflr r6
    Message_DisplayOSD 8, 0, 1, 0

    b FighterSpecificTech_End

# --------

FoxFalco_SideBEnd:
    # Check If Pressed B
    lwz r3, 0x668(playerdata)
    rlwinm. r3, r3, 0, 22, 22
    beq FighterSpecificTech_End

    li r3, OSD.FighterSpecificTech  # ID
    lbz r4, 0xC(playerdata)         # queue
    load r5, MSGCOLOR_RED
    bl FoxFalco_ShortenLatePressText
    mflr r6
    Message_DisplayOSD 8, 0, 1, 0

    b FighterSpecificTech_End

# --------

FoxFalco_ShineAirStartup:
    # ensure we just jumped from the ground
    lhz r3, 0x23fc(playerdata)
    cmpwi r3, ASID_JumpF
    beq FoxFalco_ShineAirStartup_AfterJump 
    cmpwi r3, ASID_JumpB
    beq FoxFalco_ShineAirStartup_AfterJump 
    b FighterSpecificTech_End 
     
FoxFalco_ShineAirStartup_AfterJump:

    # Calculate frames holding jump
    li r9, 0
FoxFalco_ShineAirStartup_JumpInputsLoop:
    mulli r10, r9, 8
    add r10, r10, playerdata

    lhz r11, TM_Inputs(r10) # buttons
    andi. r11, r11, 0xC00 # x or y
    bne FoxFalco_ShineAirStartup_EndJumpInputsLoop

    addi r10, r10, 3
    lbz r11, TM_Inputs(r10) # stick y
    extsb r11, r11
    cmpwi r11, 44
    bgt FoxFalco_ShineAirStartup_EndJumpInputsLoop

    # no jump input this frame
    addi r9, r9, 1
    cmpwi r9, 32
    blt FoxFalco_ShineAirStartup_JumpInputsLoop
    
    # Out of input history with no jump input - probably took longer than 32 frames.
    # Nothing we can do here unless we increase the input history range.
    b FighterSpecificTech_End
    
FoxFalco_ShineAirStartup_EndJumpInputsLoop:
    lbz r10, 0x685(playerdata) # timer_jump
    sub r9, r10, r9 # get jump held frames
    addi r9, r9, 1 # 1-index
    
    # get hop type and text
    mr r3, r9
    bl IntToFloat
    lfs f2, 0x148(playerdata) # jump_startup_time
    bl ShortHopText
    mflr r8
    li r19, 0 # save for later
    fcmpo cr0, f1, f2
    blt EndGetHopTypeText
    bl FullHopText
    mflr r8
    li r19, 1 # save for later
EndGetHopTypeText:
    mr r25, r9 # Preserve printed hop duration, not JC Shine timing in r7.
    
    # display
    li r3, OSD.FighterSpecificTechAlt  # ID - use alt so that Jump Out Of Shine does not overwrite
    lbz r4, 0xC(playerdata)         # queue
    li r5, MSGCOLOR_WHITE
    bl FoxFalco_JCShineText
    mflr r6
    lhz r7, 0x2408(playerdata) # frames in JumpF / JumpB
    Message_DisplayOSD 8, 1, 1, 0
    lwz r3, 0x2C(r3)
    lwz r20, MsgData_Text(r3)
    
    # set shine frame colour
    bl GreenRedColors
    mflr r5
    lhz r7, 0x2408(playerdata) # frames in JumpF / JumpB
    cmpwi r7, 2
    ble ShineWasGood
    addi r5, r5, 4
ShineWasGood:
    li r4, 1
    mr r3, r20 
    branchl r12, Text_ChangeTextColor
       
    # set hop type colour
    xori r3, r19, 1 # Helper takes short_hop=1, full_hop=0.
    mr r4, r25
    rtocbl r12, TM_WavedashHopColor
    stw r3, 0x84(sp)
    mr r3, r20
    li r4, 2
    addi r5, sp, 0x84
    branchl r12, Text_ChangeTextColor
    
    b FighterSpecificTech_End

# /////////////////////////////////////////////////////////////////////////////

Yoshi:
    lwz r3, 0x10(playerdata)  # load action state ID
    cmpwi r3, 0x159    # Parry Start
    beq Yoshi_Parry

    b FighterSpecificTech_End

# --------

Yoshi_Parry:
    bl CheckForJumpCancel
    cmpwi r3, 0x0
    beq FighterSpecificTech_End

Yoshi_PrintJumpOoParryText:
    load r5, MSGCOLOR_WHITE
    li r3, OSD.FighterSpecificTech  # ID
    lbz r4, 0xC(playerdata)         # queue
    bl Yoshi_JumpOoParryText
    mflr r6
    lhz r7, 0x23F8(playerdata)
    Message_DisplayOSD 8, 1, 1, 0
    b FighterSpecificTech_End

# /////////////////////////////////////////////////////////////////////////////

CheckForJumpCancel:
    mflr r0
    stw r0, 0x0004(sp)
    stwu sp, -0x0038(sp)
    # Check For JC
    lwz r5, -0x514C(r13)
    lfs f0, 0x0070(r5)
    lfs f1, 0x0624(playerdata)
    fcmpo cr0, f1, f0
    blt CheckForJumpCancel_CheckButtons
    lbz r3, 0x0671(playerdata)
    lwz r0, 0x0074(r5)
    cmpw r3, r0
    bge CheckForJumpCancel_CheckButtons
    li r3, 0x1
    b CheckForJumpCancel_OSD_Lockout

CheckForJumpCancel_CheckButtons:
    lwz r0, 0x0668(playerdata)
    rlwinm. r0, r0, 0, 20, 21
    beq CheckForJumpCancel_NoButtons
    li r3, 0x1
    b CheckForJumpCancel_OSD_Lockout

CheckForJumpCancel_NoButtons:
    li r3, 0x0

CheckForJumpCancel_OSD_Lockout:
    lwz r0, 0x003C(sp)
    addi sp, sp, 56
    mtlr r0
    blr

###################
## TEXT CONTENTS ##
###################

FoxFalco_ShortenEarlyPressText:
    blrl
    .string "Shorten Press\n%df Early"
    .align 2

FoxFalco_ShortenTypeText:
    blrl
    .string "Shorten Press\n%df/4f"
    .align 2

FoxFalco_ShortenLatePressText:
    blrl
    .string "Shorten Press\nLate"
    .align 2

FoxFalco_JCShineText:
    blrl
    .string "JC Shine\n%df\n%s: %df"
    .align 2

ShortHopText:
    blrl
    .string "Short Hop"
    .align 2

FullHopText:
    blrl
    .string "Full Hop"
    .align 2
    
GreenRedColors:
    blrl
    .long 0x8dff6eff                # green
    .long 0xffa2baff                # red
    .align 2

# --------

Yoshi_JumpOoParryText:
    blrl
    .string "Jump OoParry\n%df"
    .align 2

FighterSpecificTech_End:

##############################

OSD_Lockout:
    # Check enabled
    li r0, OSD.LockoutTimers
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Lockout_End

    # dont show if in the air
    lwz r3, 0xE0(playerdata) # air_state
    cmpwi r3, 1
    beq Lockout_End

    # dont show if in dtilt / dsmash
    lwz r3, 0x10(playerdata) # state_id
    cmpwi r3, ASID_AttackLw3
    beq Lockout_End
    cmpwi r3, ASID_AttackLw4
    beq Lockout_End

    # dont show if stick is vertical
    lfs f0, 0x624(playerdata) # input.stick.y
    fsubs f1, f1, f1 # zero f1
    fcmpo cr0, f0, f1
    bge Lockout_End

    lbz r7, 0x674(playerdata) # input.timer_lstick_smash_y
    li r5, MSGCOLOR_RED
    cmpwi r7, 4
    blt Lockout_InLockout
    li r5, MSGCOLOR_GREEN
    cmpwi r7, 25
    bge Lockout_End

Lockout_InLockout:
    addi r7, r7, 1 # 1-index timer
    bl LockoutText
    mflr r6

    li r3, OSD.LockoutTimers        # ID
    lbz r4, 0xC(playerdata)         # queue
    Message_DisplayOSD 12, 0, 1, 0
    b Lockout_End

LockoutText:
    blrl
    .string "DTilt Lockout\n%df"
    .align 2

Lockout_End:

##############################

OSD_DJL:
    li r0, OSD.FighterSpecificTech    # OSD ID
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq DJL_End
    
    lwz r3, 0x4(playerdata)  # load fighter Internal ID
    cmpwi r3, Peach.Int
    bne DJL_End
    
    bl DJL_Data
    .byte 0, 0, 0, 0
    .byte 0, 0, 0, 0
    .byte 0, 0, 0, 0
    .align 2
DJL_Data:
    mflr r3
    
    lbz r4, 0x685(playerdata) # input.timer_jump
    lwz r5, 0xC(playerdata) # ply
    add r3, r3, r5
    lbz r5, 0x0(r3) # frames since jump input
    lbz r7, 0x6(r3) # previous frame's input.timer_jump
    stb r4, 0x6(r3)
    
    # if just input a jump:
    # input.timer_jump holds zero while jump is held, so we need to compare with
    # the previous frame's input.timer_jump to determine when jump is first pressed.
    cmpwi r4, 0
    bne DJL_Increment
    cmpwi r7, 0
    bne DJL_Reset
    
# No jump, increment jump timer
DJL_Increment:
    cmpwi r5, 255
    beq DJL_End
    addi r5, r5, 1
    stb r5, 0x0(r3)
    b DJL_End

# We just input a jump, so reset jump timer
DJL_Reset:    
    li r6, 0
    stb r6, 0x0(r3)
    
    # ensure prev jump input was 10 frame or fewer ago
    cmpwi r5, 10
    bge DJL_End
    
DJL_Show:
    addi r7, r5, 1 # fix off-by-one    
    li r3, OSD.FighterSpecificTech # ID
    lbz r4, 0xC(playerdata)        # queue
    
    li r5, MSGCOLOR_RED
    cmpwi r7, 5
    bne DJL_EndColor
    li r5, MSGCOLOR_GREEN
DJL_EndColor:

    bl DJL_Text
    mflr r6
    Message_DisplayOSD 8, 1, 1, 0, 5 # Preserve the original best interval: five frames.
    b DJL_End
    
DJL_Text:
    blrl
    .string "Insta Double Jump\n%df"
    .align 2
    
DJL_End:
    b Exit

##############################

IntToFloat:
    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x100(r1)             # make space for 12 registers
    stmw r20, 0x8(r1)
    stfs f2, 0x38(r1)

    lis r0, 0x4330
    lfd f2, -0x6758(rtoc)
    xoris r3, r3, 0x8000
    stw r0, 0xF0(sp)
    stw r3, 0xF4(sp)
    lfd f1, 0xF0(sp)
    fsubs f1, f1, f2                # Convert To Float

    lfs f2, 0x38(r1)
    lmw r20, 0x8(r1)
    lwz r0, 0x104(r1)
    addi r1, r1, 0x100              # release the space
    mtlr r0
    blr

Exit:
    restoreall
    lwz r12, 0x219C(r31)
