    # To be inserted at 801baa98
    .include "../../../../Globals.s"
    .include "../../../../m-ex/Header.s"
    
    # RESTORE HMN ----------------------------------

    # Get Backup Location
    lwz r9, MemcardData(r13)
    addi r9, r9, 3344
    # Restore HMN Character Choice
    load r3, 0x80497758     # CSS Match Info
    # mr r4, r4 #CSS Type
    lbz r5, 104(r9)         # Character Backup
    li r6, 0                # Unk
    lbz r7, 107(r9)         # Costume Backup
    lbz r8, 114(r9)         # Nametag Backup Location
    li r9, 0                # Unk Backup Location
    lbz r10, 0x0006(r31)    # Player who accessed CSS
    branchl r12, 0x801b06b0
    
    # RESTORE CPU ----------------------------------
    
    # load allowed cpu characters
    lwz r4, MemcardData(r13)
    lbz r3, 0x0535(r4)
    SettingsRead SettingsField_Page, 4
    rtocbl r12, TM_GetEventCharList
    lwz r6, 0x4(r3)
        
    cmpwi r6, -1
    beq RestoreCPU
    
    # CPU is not allowed, find first allowed CPU
    
    li r4, 0
FindFirstAllowedCPU:
    li r5, 1
    slw r5, r5, r4
    and. r5, r5, r6
    bne FoundCPU
    addi r4, r4, 1
    b FindFirstAllowedCPU
    
FoundCPU:
    # convert from CSSID to External ID
    bl CSSIDToExternalIDMapEnd
    .byte Doc.Ext 
    .byte Mario.Ext 
    .byte Luigi.Ext 
    .byte Bowser.Ext 
    .byte Peach.Ext 
    .byte Yoshi.Ext 
    .byte DK.Ext 
    .byte CaptainFalcon.Ext 
    .byte Ganondorf.Ext 
    .byte Falco.Ext 
    .byte Fox.Ext 
    .byte Ness.Ext 
    .byte IceClimbers.Ext 
    .byte Kirby.Ext 
    .byte Samus.Ext 
    .byte Zelda.Ext 
    .byte Link.Ext 
    .byte YLink.Ext 
    .byte Pichu.Ext 
    .byte Pikachu.Ext 
    .byte Jigglypuff.Ext 
    .byte Mewtwo.Ext 
    .byte GaW.Ext 
    .byte Marth.Ext 
    .byte Roy.Ext
    .align 2 
CSSIDToExternalIDMapEnd:
    mflr r6
    add r4, r4, r6
    lbz r4, 0x0(r4)

    li r6, 0 # set first costume
    
    # Get Backup Location
    lwz r9, MemcardData(r13)
    addi r9, r9, 3344
    b WriteCPU
    
RestoreCPU:
    # Get Backup Location
    lwz r9, MemcardData(r13)
    addi r9, r9, 3344
    
    lbz r6, 143(r9)         # Costume Backup
    lbz r4, 140(r9)         # Character Backup

    # Swap sheik to zelda
    cmpwi r4, Sheik.Ext
    bne WriteCPU
    li r4, Zelda.Ext

WriteCPU:
    load r3, 0x80497758     # CSS Match Info
    li r5, 1                # Unk
    lbz r7, 150(r9)         # Nametag Backup
    li r8, 0
    lbz r9, 0x0006(r31)     # Player who accessed CSS
    branchl r12, 0x801b07b4
