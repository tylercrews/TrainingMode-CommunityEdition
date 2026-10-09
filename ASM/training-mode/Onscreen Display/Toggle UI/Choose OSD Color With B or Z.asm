    # To be inserted at 80236000
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    # Handle palette input before RSS's Boolean toggle path. The custom editor
    # keeps its Boolean snapshot synchronized, so native exit saving is harmless.
    SettingsBackup
    load r5, 0x804a04f0
    lbz r0, 0x11(r5)
    cmpwi r0, 2
    blt original
    lhz r5, 2(r5) # Cursor index in the native RSS order.
    mr r3, r29    # RSS userdata (row bytes + 2, Text pointers + 0x40/+0x44).
    mr r4, r28    # Instant input mask already fetched by native think.
    rtocbl r12, TM_OSDEditorInput
    cmpwi r3, 0
    beq original
    li r3, 2
    branchl r12, SFX_MenuCommonSound
    SettingsRestore
    branch r12, 0x80236164 # Skip Boolean toggle/save and finish native think.

original:
    SettingsRestore
    rlwinm. r0, r28, 0, 22, 22
