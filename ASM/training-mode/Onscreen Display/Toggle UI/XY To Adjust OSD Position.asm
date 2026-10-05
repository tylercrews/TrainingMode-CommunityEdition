    # To be inserted at 802360f8
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    .set Text, 30
    .set TextProp, 28

    .set OptionCount, 4

# Injected into CursorMovement Check

    backup

    # CHECK FLAG IN RULES STRUCT
    load r4, 0x804a04f0
    lbz r0, 0x0011(r4)
    cmpwi r0, 0x2
    blt exit

    li r3, 4
    branchl r12, Inputs_GetPlayerInstantInputs

    lwz r20, MemcardData(r13)       # Get Memcard Data

#############################################
CheckY:
    rlwinm. r0, r4, 0, 20, 20   # Check For Y
    beq CheckX

    # Increase Number
    SettingsRead SettingsField_Position, 3
    addi r3, r3, 1
    # Check If Over Max
    cmpwi r3, OptionCount
    blt CheckY_Store
    # Loop Back to 0
    li r3, 0

CheckY_Store:
    SettingsWrite SettingsField_Position, 3

    b UpdateText

#############################################

CheckX:
    rlwinm. r0, r4, 0, 21, 21   # Check For X
    beq exit

    # Decrease Number
    SettingsRead SettingsField_Position, 3
    subi r3, r3, 1
    # Check If Over Max
    cmpwi r3, 0
    bge CheckX_Store
    # Loop Back to OptionCount
    li r3, OptionCount-1

CheckX_Store:
    SettingsWrite SettingsField_Position, 3

    b UpdateText

#############################################

UpdateText:
    lwz r3, 0x40(r29)           # Get Text Structure
    lbz r4, 0x48(r29)           # Get Subtext ID
    bl OSDPositionText
    mflr r5

    SettingsRead SettingsField_Position, 6
    cmpwi r6, 0
    beql OSDPositionTextHUD
    cmpwi r6, 1
    beql OSDPositionTextSides
    cmpwi r6, 2
    beql OSDPositionTextTopLeft
    cmpwi r6, 3
    beql OSDPositionTextTopRight
    mflr r6

    branchl r12, Text_UpdateSubtextContents

    b PlaySFX

#############################################

OSDPositionText:
    blrl
    .string "OSD Position: %s"
    .align 2

OSDPositionTextHUD:
    blrl
    .string "HUD"
    .align 2

OSDPositionTextSides:
    blrl
    .string "Sides"
    .align 2

OSDPositionTextTopLeft:
    blrl
    .string "Top Left"
    .align 2

OSDPositionTextTopRight:
    blrl
    .string "Top Right"
    .align 2

########################################

PlaySFX:
    li r3, 2
    branchl r12, SFX_MenuCommonSound

exit:
    restore
    rlwinm. r0, r28, 0, 23, 23
