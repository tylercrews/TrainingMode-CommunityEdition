#include "events.h"

/* Match the native menu-font width calculation at 0x803A8368: glyphs
 * 0x2000..0x211E use 32 + 2 - left/right kerning. Measuring at creation lets
 * separately colored runs meet without an artificial gap or overlap. */
static float compact_width(char *ascii) {
    u8 glyphs[64];
    int bytes = Text_ConvertToMenuText((char *)glyphs, ascii);
    const u8 *kerning = (const u8 *)0x8040CB00;
    float width = 0;
    for (int i = 0; i + 1 < bytes; i += 2) {
        unsigned code = ((unsigned)glyphs[i] << 8) | glyphs[i + 1];
        if (code < 0x2000 || code >= 0x211F) continue;
        unsigned index = (code - 0x2000) * 2;
        width += 34 - kerning[index] - kerning[index + 1];
    }
    return width;
}
void OSD_FormatTiming(MsgData *msg, int inline_layout, int y) {
    if (msg->timing_frame < 0) return;
    float center = inline_layout ? 55 : 0, scale = 0.7f;
    if (inline_layout) {
        Text_SetText(msg->text, 0, "Wavedash");
        Text_SetPosition(msg->text, 0, -70, y);
        msg->timing_subtext = Text_AddSubtext(msg->text, center, y, "%df", msg->timing_frame);
        Text_SetScale(msg->text, 0, scale, scale);
        Text_SetScale(msg->text, msg->timing_subtext, scale, scale);
    } else y += msg->timing_subtext * MSGTEXT_YOFFSET;
    if (msg->timing_hitlag) {
        char prefix[24], result[24];
        /* No right-arrow exists in the native 287-glyph lookup. ASCII -> is
         * the supported fallback, with no surrounding spaces. */
        sprintf(prefix, "%dhl->", msg->timing_hitlag);
        sprintf(result, msg->settings_id == 1 ? "%df/7f" : "%df", msg->timing_frame);
        float prefix_width = compact_width(prefix) * scale;
        float result_width = compact_width(result) * scale;
        Text_SetText(msg->text, msg->timing_subtext, result);
        Text_SetPosition(msg->text, msg->timing_subtext, center + prefix_width / 2, y);
        Text_SetScale(msg->text, msg->timing_subtext, scale, scale);
        msg->timing_prefix = Text_AddSubtext(msg->text, center - result_width / 2, y, prefix);
        Text_SetScale(msg->text, msg->timing_prefix, scale, scale);
    }
}

void OSD_ApplyMessageStyle(MsgData *msg) {
    if (msg->settings_id < 0) return;
    unsigned choice = Settings_Get(TM_SETTING_OSD_COLOR, msg->settings_id);
    uint32_t rgba = OSD_PaletteColor(choice);
    GXColor title = {rgba >> 24, rgba >> 16, rgba >> 8, rgba};
    Text_SetColor(msg->text, 0, &title);
    if (msg->timing_frame >= 0) {
        rgba = OSD_TimingColorFor(msg->timing_frame, msg->timing_best);
        GXColor result = {rgba >> 24, rgba >> 16, rgba >> 8, rgba};
        Text_SetColor(msg->text, msg->timing_subtext, &result);
    }
    if (msg->timing_prefix >= 0) {
        GXColor neutral = {255,255,255,255};
        Text_SetColor(msg->text, msg->timing_prefix, &neutral);
    }
}
void OSD_MessageGX(GOBJ *gobj, int pass) {
    MsgData *msg = gobj->userdata;
    int visible = msg->settings_id < 0 ||
        (!Settings_Get(TM_SETTING_FLAG, TM_FLAG_OSDS_OFF) &&
         Settings_Get(TM_SETTING_OSD_COLOR, msg->settings_id) != TM_COLOR_OFF);
    if (visible && (unsigned)msg->queue_num < 6 && Settings_Get(TM_SETTING_FLAG, TM_FLAG_CPU_OSDS_OFF)) {
        Playerblock *player = Fighter_GetPlayerblock(msg->queue_num);
        if (player && player->p_kind == 1) visible = 0;
    }
    msg->text->hidden = !visible;
    if (!visible) return; /* Preserve the valid GOBJ contract even when a category is Off. */
    OSD_ApplyMessageStyle(msg); /* After all legacy caller recoloring, before the text GX pass. */
    GXLink_Common(gobj, pass);
}
