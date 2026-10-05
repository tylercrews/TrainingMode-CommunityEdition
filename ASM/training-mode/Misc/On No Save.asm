    # To be inserted at 801af6f4
    .include "../../Globals.s"
    .include "../../../build/tyro-identity.s"

    .set MEMCARD_HASSAVE, 0x0
    .set MEMCARD_NOSAVE, 0x4
    .set MEMCARD_NONE, 0xF
    .set MEMCARD_NONE2, 0xD

    # Skip if we have save
    cmpwi r29, MEMCARD_HASSAVE
    beq Original

InitSave:
    backupall
    mfctr r7
    stw r7, 0x80(r1)
    lwz r4, MemcardData(r13)
    # Fresh/no-card Tyro starts must not inherit stale overlay/signature bytes.
    # Do this before calling any C service: the file loader may not be ready yet.
    lis r5, 0x8000
    lwz r5, 0(r5)
    load r6, TyroGameCode
    cmpw r5, r6
    bne LegacyDefaults
    addi r5, r4, 0x1F24
    li r6, 11
    li r3, 0
    mtctr r6
ClearTyroSettings:
    stw r3, 0(r5)
    addi r5, r5, 4
    bdnz ClearTyroSettings
LegacyDefaults:
    # Set Max OSD on No Memcard
    li r3, 1
    stb r3, OSDMaxWindows(r4)
    # Set Initial Page Number
    li r3, 1
    stb r3, CurrentEventPage(r4)
    # Enable Recommended OSDs
    li r3, 0
    stb r3, OSDRecommended(r4)
    # Turn off OSDs by default
    li r3, 0
    stw r3, OSDBitfield(r4)

    lwz r7, 0x80(r1)
    mtctr r7
    restoreall

    cmpwi r29, MEMCARD_NOSAVE
    beq Original

NoMemcard:
    # No memcard available
    # Exit memcard think and disable saving
    branch r12, 0x801b01ac

Original:
    cmpwi r29, 0
