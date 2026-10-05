    # To be inserted at 8024d84c
    .include "../../../../Globals.s"
    .include "../../../../m-ex/Header.s"

    .set MainTextOffset, -0x4EB4
    .set LeftArrowTextOffset, -0x4EB0
    .set RightArrowTextOffset, -0x4EAC

    backup

    .set REG_PageNum, 22
    .set EventID, 23
    .set PageID, 24

    # Get number of pages
    rtocbl r12, TM_GetPageNum
    mr REG_PageNum, r3
    # Get Event ID
    lbz r3, 0x0(r31)
    lwz r4, 0x4(r31)
    add EventID, r3, r4
    # Check if first page
    lwz r3, MemcardData(r13)
    SettingsRead SettingsField_Page, PageID
    cmpwi PageID, 0
    bne ShowFirstPage
    # Hide left arrow
    li r4, 1
    b FirstPageVisibility

ShowFirstPage:
    # Show left arrow
    li r4, 0
    b FirstPageVisibility

FirstPageVisibility:
    lwz r3, LeftArrowTextOffset (r13)
    stb r4, 0x4D(r3)

    # Check if last page
    cmpw PageID, REG_PageNum
    bne ShowLastPage
    # Hide right arrow
    li r4, 1
    b LastPageVisibility

ShowLastPage:
    # Show right arrow
    li r4, 0
    b LastPageVisibility

LastPageVisibility:
    lwz r3, RightArrowTextOffset (r13)
    stb r4, 0x4D(r3)

end:
    restore
    lwz r0, 0x0024(sp)
