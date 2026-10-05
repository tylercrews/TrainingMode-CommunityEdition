    # To be inserted at 80236e00
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    # CHECK FLAG IN RULES STRUCT
    load r4, 0x804a04f0
    lbz r0, 0x0011(r4)
    cmpwi r0, 0x2
    blt original

    # Native row mapping keeps global trail flags separate from the OSD mask.
    rlwinm r3, r3, 0, 16, 31
    SettingsReadIndexed SettingsField_Row, 3, 3
    b exit

original:
    branchl r4, 0x80164250

exit:
