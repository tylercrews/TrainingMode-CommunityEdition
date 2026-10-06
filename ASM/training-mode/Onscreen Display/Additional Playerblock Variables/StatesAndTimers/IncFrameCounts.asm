    # To be inserted at 8006ab78
    .include "../../../../Globals.s"
    .include "../../../../m-ex/Header.s"

# Inc Frame Count + Frames Tangible + Hitstun

    # Capture actual frozen updates before discarding them from native counters.
    SettingsBackup
    mr r3, r31
    rtocbl r12, TM_OSDContextTick
    SettingsRestore

    # Don't Run During Hitlag
    lbz r0, 0x2219(r31)
    rlwinm. r0, r0, 30, 31, 31
    bne exit

########################
## Increment AS Frame ##
########################

    lhz r3, TM_FramesinCurrentAS(r31)
    addi r3, r3, 0x1
    sth r3, TM_FramesinCurrentAS(r31)

##################################
## Increment Tangibility Frames ##
##################################

    # Check Timer Intangible Count
    lwz r3, 0x1990(r31)
    cmpwi r3, 0x0
    bgt IsIntangible

    # Ensure Player Had Ledge Intang FIRST
    lhz r3, TM_TangibleFrameCount(r31)
    cmpwi r3, 0x0
    bne IncTangibleFrames
    # Check For Subaction Intang
    lwz r3, 0x1988(r31)
    cmpwi r3, 0x2
    bne IncTangibleFrames

# Set Tangible Frames to 0 When Player Has Intangibility
IsIntangible:
    li r3, 0x0
    sth r3, TM_TangibleFrameCount(r31)  # Store Tangible Frame Count
    b Tangible_End

IncTangibleFrames:
    lhz r3, TM_TangibleFrameCount(r31)
    addi r3, r3, 0x1
    sth r3, TM_TangibleFrameCount(r31)

Tangible_End:

###################
## Update Inputs ##
###################

InputsShiftOld:
    addi r4, r31, TM_Inputs    # Get to the Start of the Inputs
    addi r3, r4, 8
    li r5, 0xF8                # 0x100 - 8
    branchl r6, memmove
InputsWriteNew:
    load r3, 0x804c21cc  # engine pad array
    lbz r4, 0x618(r31)   # pad_index
    mulli r4, r4, 0x44   # r4 *= sizeof(HSD_Pad)
    add r3, r3, r4
    # ptr to HSD_pad now in r3

    addi r5, r31, TM_Inputs    # Get to the Start of the Inputs
    lwz r4, 0x0(r3) # held
    sth r4, 0x0(r5)

    lbz r4, 0x18(r3) # stick_x
    stb r4, 0x2(r5)
    lbz r4, 0x19(r3) # stick_y
    stb r4, 0x3(r5)
    lbz r4, 0x1A(r3) # cstick_x
    stb r4, 0x4(r5)
    lbz r4, 0x1B(r3) # cstick_y
    stb r4, 0x5(r5)
    lbz r4, 0x1C(r3) # l_trigger
    stb r4, 0x6(r5)
    lbz r4, 0x1D(r3) # r_trigger
    stb r4, 0x7(r5)

exit:
    mr r3, r30
