    # To be inserted at 8000551c
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    # Shared implementation includes regular landing, direct recovery exits,
    # character-specific normal lag, recovery labels and hitlag metadata.
    SettingsBackup
    rtocbl r12, TM_ActOutWait
    SettingsRestore
    blr
