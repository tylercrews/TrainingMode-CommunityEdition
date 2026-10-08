#ifndef TM_MENU_CONTROLLER_LAYOUT_H
#define TM_MENU_CONTROLLER_LAYOUT_H

/* SIS glyph cells are at most 32 units high. Leave an entire cell for the
 * final preview row and use baseline spacing larger than its rendered height. */
#define MC_PREVIEW_LINES 24
#define MC_DESC_LINES 8
#define MC_BODY_Y (-218)
#define MC_DESC_STEP 30
#define MC_PREVIEW_STEP 32
#define MC_PREVIEW_BOTTOM 282
#define MC_WRAP_CHARS 46
#define MC_DETAIL_SCALE .84f

static inline int MenuController_PreviewStart(int description_lines)
{
    return MC_BODY_Y + description_lines * MC_DESC_STEP + 22;
}
static inline int MenuController_PreviewCapacity(int first_y)
{
    int lines = (MC_PREVIEW_BOTTOM - first_y - 32) / MC_PREVIEW_STEP + 1;
    if (lines < 3) lines = 3;
    return lines > MC_PREVIEW_LINES ? MC_PREVIEW_LINES : lines;
}
#endif
