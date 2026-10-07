    # To be inserted at 8006b80c
    .include "../../Globals.s"
    .include "../../m-ex/Header.s"

    # The native IASA callback has returned. Observe actual turn/jump outcomes.
    SettingsBackup
    mr r3, r30
    rtocbl r12, TM_ShineAfterIASA
    SettingsRestore
    lwz r0, 0x7c(r1)              # Original epilogue instruction.
