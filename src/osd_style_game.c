#include "events.h"

/* ESC followed by eight uppercase RGBA hex digits emits native 0x0C + RGB.
 * The native command has no alpha byte. One centered row owns every run. */
static char *append_color(char *out, uint32_t color) {
    static const char hex[] = "0123456789ABCDEF";
    *out++ = 0x1B;
    for (int shift = 28; shift >= 0; shift -= 4) *out++ = hex[(color >> shift) & 15];
    return out;
}
static char *append_text(char *out, const char *in) {
    while (*in) *out++ = *in++;
    return out;
}
void OSD_TimingText(MsgData *msg, char *line, int which) {
    char part[24];
    char *out = line;
    if (which != 2 && msg->timing_hitlag) {
        out = append_color(out, 0xFFFFFFFF);
        sprintf(part, "%dhl->", msg->timing_hitlag);
        out = append_text(out, part);
    }
    if (which != 2 && msg->timing_turn) {
        out = append_color(out, OSD_TimingColor(msg->timing_turn));
        sprintf(part, "%dtrn->", msg->timing_turn);
        out = append_text(out, part);
    }
    if (which != 1) {
    out = append_color(out, msg->timing_second_turn || msg->timing_failed ? 0xFFA2BAFF :
        OSD_TimingColorFor(msg->timing_frame, msg->timing_best));
    if (msg->timing_failed) strcpy(part, "FAIL");
    else sprintf(part, msg->timing_second_turn ? "%dtrn" : msg->settings_id == 1 ? "%df/7f" : "%df", msg->timing_frame);
    out = append_text(out, part);
    }
    *out = 0;
}
void OSD_FormatTiming(MsgData *msg, int inline_layout, int y) {
    if (msg->timing_frame < 0) return;
    if (!inline_layout && !msg->timing_hitlag && !msg->timing_turn && !msg->timing_encoded && !msg->timing_failed) return;
    char line[128], body[112];
    OSD_TimingText(msg, body, 0);
    if (inline_layout) { sprintf(line, "Wavedash %s", body); msg->timing_subtext = 0; }
    else strcpy(line, body);
    Text_SetText(msg->text, msg->timing_subtext, line);
    Text_SetPosition(msg->text, msg->timing_subtext, 0, inline_layout ? y : y + msg->timing_subtext * MSGTEXT_YOFFSET);
    Text_SetScale(msg->text, msg->timing_subtext, 1.f, 1.f);
    msg->timing_prefix = -1; msg->timing_encoded = 1;
}

void OSD_ApplyMessageStyle(MsgData *msg) {
    if (msg->settings_id < 0) return;
    unsigned choice = Settings_Get(TM_SETTING_OSD_COLOR, msg->settings_id);
    uint32_t rgba = OSD_PaletteColor(choice);
    GXColor title = {rgba >> 24, rgba >> 16, rgba >> 8, rgba};
    Text_SetColor(msg->text, 0, &title);
    if (msg->timing_frame >= 0 && !msg->timing_encoded) {
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
    int visible = Message_LayoutVisible(msg) && (msg->settings_id < 0 ||
        (!Settings_Get(TM_SETTING_FLAG, TM_FLAG_OSDS_OFF) &&
         Settings_Get(TM_SETTING_OSD_COLOR, msg->settings_id) != TM_COLOR_OFF));
    if (visible && (unsigned)msg->queue_num < 6 && Settings_Get(TM_SETTING_FLAG, TM_FLAG_CPU_OSDS_OFF)) {
        Playerblock *player = Fighter_GetPlayerblock(msg->queue_num);
        if (player && player->p_kind == 1) visible = 0;
    }
    msg->text->hidden = !visible;
    if (msg->layout_footer) msg->layout_footer->hidden = !visible;
    if (!visible) return; /* Preserve the valid GOBJ contract even when a category is Off. */
    OSD_ApplyMessageStyle(msg); /* After all legacy caller recoloring, before the text GX pass. */
    Message_LayoutGeometry(gobj);
    GXLink_Common(gobj, pass);
}
