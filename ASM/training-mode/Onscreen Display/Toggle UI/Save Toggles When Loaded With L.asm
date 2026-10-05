    # To be inserted at 80235994
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    # CHECK FLAG IN RULES STRUCT
    load r5, 0x804a04f0
    lbz r0, 0x0011(r5)
    cmpwi r0, 0x2
    blt original

    # Preserve palette choices/unknown IDs and respect unsupported-format fallback.
    rlwinm r3, r3, 0, 24, 31
    rlwinm r4, r4, 0, 24, 31
    SettingsToggle 3, 4
    b exit

original:
    branchl r5, 0x801641e4

exit:
