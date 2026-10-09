    # To be inserted at 801bab28
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    stb r0, 0x000A(r31)

    # Get Event ID
    lwz r3, MemcardData(r13)
    lbz r4, 0x0535(r3)
    SettingsRead SettingsField_Page, 3
    # Check if this event has a SSS
    rtocbl r12, TM_GetIsSelectStage
    cmpwi r3, 0
    beq original

SSS:
    # Store SSS as Next Scene
    load r3, SceneController
    li r4, 0x3
    stb r4, 0x5(r3)

original:
