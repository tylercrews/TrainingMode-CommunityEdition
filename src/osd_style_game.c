#include "events.h"

void OSD_FormatTiming(MsgData *msg, int inline_layout, int y) {
    if (msg->timing_frame < 0) return;
    if (inline_layout) {
        /* Restore the compact first row; leave angle/hop positions and sizes
         * intact. Both title and timing runs fit in the existing three lines. */
        Text_SetText(msg->text, 0, "Wavedash");
        Text_SetPosition(msg->text, 0, msg->timing_hitlag ? -100 : -70, y);
        msg->timing_subtext = Text_AddSubtext(msg->text, msg->timing_hitlag ? 85 : 55, y, "Frame %d", msg->timing_frame);
        Text_SetScale(msg->text, 0, 0.7f, 0.7f);
        Text_SetScale(msg->text, msg->timing_subtext, msg->timing_hitlag ? 0.55f : 0.7f,
            msg->timing_hitlag ? 0.55f : 0.7f);
    }
    if (msg->timing_hitlag) {
        if (!inline_layout) {
            y += msg->timing_subtext * MSGTEXT_YOFFSET;
            Text_SetPosition(msg->text, msg->timing_subtext, 65, y);
            /* Keep L-cancel's /7 window and its separate outcome line. */
            Text_SetText(msg->text, msg->timing_subtext, msg->settings_id == 1 ? "Frame %d/7" : "Frame %d", msg->timing_frame);
            Text_SetScale(msg->text, msg->timing_subtext, 0.7f, 0.7f);
        }
        msg->timing_prefix = Text_AddSubtext(msg->text, inline_layout ? -15 : -65, y, "%dhl ->", msg->timing_hitlag);
        Text_SetScale(msg->text, msg->timing_prefix, inline_layout ? 0.55f : 0.7f, inline_layout ? 0.55f : 0.7f);
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
    msg->text->hidden = !visible;
    if (!visible) return; /* Preserve the valid GOBJ contract even when a category is Off. */
    OSD_ApplyMessageStyle(msg); /* After all legacy caller recoloring, before the text GX pass. */
    GXLink_Common(gobj, pass);
}
