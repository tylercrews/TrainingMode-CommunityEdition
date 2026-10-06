#include "events.h"

/* Native RSS cursor order, not sparse OSD ID order. The first 15 rows are left. */
static const uint8_t row_ids[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 10, 24, 26, 28,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 25, 27,
};
static const char *row_names[] = {
    "Wavedash Info", "L-Cancel", TM_GLOBAL_TRAIL_VERY_FAST_NAME, "Act OoS Frame",
    TM_GLOBAL_TRAIL_INSTANT_NAME, "Dashback", TM_GLOBAL_OSDS_OFF_NAME, 0,
    "Fighter-specific Tech", "Powershield Frame", 0, "SDI Inputs", "Grab Breakout",
    "Ledgedash Info", "Act OoHitstun", "Lockout Timers", "Item Throw Interrupts",
    "Boost Grab", 0, "Act OoLag", 0, "Act OoAirborne", "Jump Cancel Timing",
    "Fastfall Timing", "Frame Advantage", "Combo Counter", 0, 0, 0,
};
static const char *color_names[] = { TM_OSD_COLOR_NAMES };

static int flag_for(unsigned id) {
    if (id == TM_GLOBAL_TRAIL_VERY_FAST_ROW) return TM_FLAG_TRAILS_VERY_FAST;
    if (id == TM_GLOBAL_TRAIL_INSTANT_ROW) return TM_FLAG_TRAILS_INSTANT;
    if (id == TM_GLOBAL_OSDS_OFF_ROW) return TM_FLAG_OSDS_OFF;
    return -1;
}

static void refresh_row(void *data, unsigned row) {
    unsigned id = row_ids[row];
    int flag = flag_for(id);
    unsigned value = Settings_Get(flag < 0 ? TM_SETTING_OSD_COLOR : TM_SETTING_FLAG,
                                  flag < 0 ? id : (unsigned)flag);
    Text *text = *(Text **)((uint8_t *)data + (row < 15 ? 0x40 : 0x44));
    int subtext = (row < 15 ? row : row - 15) + 1;
    const char *choice = flag < 0 ? color_names[value & 7] : (value ? "On" : "Off");
    Text_SetText(text, subtext, "%s: %s", row_names[row], choice);
    /* Fit long names plus the palette value into the existing two-column layout. */
    Text_SetScale(text, subtext, 0.78f, 1.0f);
    uint32_t rgba = value ? (flag < 0 ? OSD_PaletteColor(value) : 0x8DFF6EFF) : 0xB4B4B4FF;
    GXColor color = {rgba >> 24, rgba >> 16, rgba >> 8, rgba};
    Text_SetColor(text, subtext, &color);
}

void OSD_EditorInit(void *data) {
    for (unsigned row = 0; row < sizeof(row_ids); ++row)
        if (row_names[row]) {
            refresh_row(data, row);
            ((uint8_t *)data)[row + 2] = !!Settings_Get(TM_SETTING_NATIVE_ROW, row_ids[row]);
        }
}

/* Return whether B/Z was consumed, including on unused rows. A/Start still exit,
 * D-pad/stick still navigate and X/Y still choose the screen position. */
int OSD_EditorInput(void *data, unsigned buttons, unsigned row) {
    if (!(buttons & (HSD_BUTTON_B | HSD_TRIGGER_Z))) return 0;
    if (row >= sizeof(row_ids) || !row_names[row]) return 1;
    unsigned id = row_ids[row];
    int flag = flag_for(id);
    unsigned field = flag < 0 ? TM_SETTING_OSD_COLOR : TM_SETTING_FLAG;
    unsigned index = flag < 0 ? id : (unsigned)flag;
    unsigned value = Settings_Get(field, index);
    value = flag < 0 ? (value + ((buttons & HSD_BUTTON_B) ? 1 : 7)) & 7 : !value;
    Settings_Set(field, index, value);
    refresh_row(data, row);
    /* Leave the row byte at its old value: native think detects this difference,
     * animates the checkbox, then copies the selected Boolean into that byte. */
    *(volatile uint8_t *)0x804A04F4 = !!value;
    return 1;
}
