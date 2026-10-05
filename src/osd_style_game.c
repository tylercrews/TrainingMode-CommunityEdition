#include "events.h"

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
}
void OSD_MessageGX(GOBJ *gobj, int pass) {
    MsgData *msg = gobj->userdata;
    int visible = msg->settings_id < 0 || Settings_Get(TM_SETTING_OSD_COLOR, msg->settings_id) != TM_COLOR_OFF;
    msg->text->hidden = !visible;
    if (!visible) return; /* Preserve the valid GOBJ contract even when a category is Off. */
    OSD_ApplyMessageStyle(msg); /* After all legacy caller recoloring, before the text GX pass. */
    GXLink_Common(gobj, pass);
}
