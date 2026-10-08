    # To be inserted at 8024e858
    .include "../../../Globals.s"

    # Native initialization uses r30 to position the cursor and scroll window.
    # Restore through that path so names, description and scores agree.
    SettingsBackup
    SettingsRead SettingsField_Page, 3
    rtocbl r12, TM_GetPageEventNum
    mr r31, r3
    SettingsRead SettingsField_EventSelection, 3
    cmpw r3, r31
    ble ValidSelection
    li r3, 0
    SettingsWrite SettingsField_EventSelection, 3
ValidSelection:
    # SettingsBackup/SettingsRestore restores r30; patch its saved slot with the selected ID.
    stw r3, (0x30 + (30 - 3) * 4)(sp)
    SettingsRestore
    # Original instruction (addi r30,r3,0) is replaced by restored r30.
