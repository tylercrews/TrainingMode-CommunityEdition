    # To be inserted at 800da07c
    .include "../../Globals.s"

    .set playerdata, 31

    backupall

    # CHECK IF ENABLED
    li r0, OSD.GrabBreakout # OSD Menu ID
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

    # The grab timer is set at some number when initially grabbed,
    # then decrements by 1 for each frame not being pummelled, and for each input.
    # The grab breaks when the timer reaches zero.
    bl Data
    .float 0.5
    .float 1.0
    .long 0    # 0x8  initial grab timer
    .long 0    # 0xC  frame counter for grab
    .long 0    # 0x10 global frame counter - used to detect new grabs
Data:
    mflr r3

    # If this run is not 1f later than last run, then it's a new grab.
    # Doesn't work for simultaneous grabs in doubles, oh well.
CheckNewGrab:
    # r5 = stc_match->time_frames
    lis r5, 0x8046
    ori r5, r5, 0xb6a0
    lwz r5, 0x24(r5)

    lwz r4, 0x10(r3)
    stw r5, 0x10(r3)
    addi r4, r4, 1
    cmpw r4, r5

    beq GrabUpdate

NewGrab:
    # initial_grab_timer = data->grab.grab_timer
    lfs f0, 0x1A4C (playerdata)
    stfs f0, 0x8(r3)
    li r4, 0
    stw r4, 0xC(r3)

GrabUpdate:
    # frame_counter += 1.f
    lfs f1, 0xC(r3)
    lfs f2, 0x4(r3)
    fadds f1, f1, f2
    stfs f1, 0xC(r3)

    # grab_timer = data->grab.grab_timer
    lfs f1, 0x1A4C (playerdata)

    # calculate mash rate
    # mash_rate = (initial_grab_timer - grab_timer) / frame_counter
    lfs f2, 0x8(r3)
    lfs f3, 0xC(r3)
    fsubs f2, f2, f1
    fdivs f2, f2, f3

    li r3, OSD.GrabBreakout
    lbz r4, 0xC(playerdata)
    li r5, MSGCOLOR_WHITE
    bl Text
    mflr r6
    Message_DisplayOSD 24, 0, 1, 0
    b Exit

Text:
    blrl
    .string "Grab Breakout\nGrab Timer: %.0f\nMash Rate: %.1f/f"
    .align 2

Exit:
    restoreall
    lbz r0, 0x2226 (r31)
