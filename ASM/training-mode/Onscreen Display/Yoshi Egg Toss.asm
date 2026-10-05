    # To be inserted at 8012e208
    # Inserted right after call to Projectile_YoshiEggThrow_CalculateAngle
    # f6 is angle, f3 is press duration, r31 is fighter data

    .include "../../Globals.s"
    .include "../../m-ex/Header.s"

    backupall

    # ensure enabled
    li r0, OSD.FighterSpecificTech
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Exit

    bl Data
    mflr r3

    # mirror if facing left: angle = pi-angle
    lfs f2, 0x2C(r31) # facing direction
    fsubs f1, f1, f1  # zero f1
    fcmpo cr0, f2, f1
    bgt SkipMirrorAngle

    lfs f1, 0x4(r3)
    fsubs f6, f1, f6

SkipMirrorAngle:
    lfs f1, 0x0(r3)
    fdivs f1, f6, f1
    fmr f2, f3

    li r3, OSD.FighterSpecificTech
    lbz r4, 0xC(r31)
    bl Message
    mflr r6
    li r5, MSGCOLOR_WHITE

    Message_DisplayOSD 8, 0, 1, 0
    b Exit

Data:
    blrl
    .float 0.0174533 # radians / degrees ratio
    .float 3.1415926 # pi

Message:
    blrl
    .string "Egg Toss\nAngle: %.0f\nStrength: %.0f"
    .align 2

Exit:
    restoreall
    lwz r3, 0x2344 (r31)
