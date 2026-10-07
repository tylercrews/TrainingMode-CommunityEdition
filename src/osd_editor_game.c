#include "events.h"

/* Native RSS cursor order, not sparse OSD ID order. The first 15 rows are left. */
#define row_ids TMSettings_EditorIDs
static const char *row_names[] = {
    "Wavedash Info", "L-Cancel", "Act OoS Frame", "Dashback", "Fighter-specific Tech",
    "Powershield Frame", "SDI Inputs", "Lockout Timers", "Item Throw Interrupts", "Boost Grab",
    "Act OoLag", "Act OoAirborne", "Jump Cancel Timing", "Fastfall Timing", "Frame Advantage",
    "Combo Counter", "Grab Breakout", "Ledgedash Info", "Act OoHitstun",
    TM_GLOBAL_CPU_OSDS_OFF_NAME, TM_GLOBAL_OSDS_OFF_NAME, 0, TM_GLOBAL_TRAIL_VERY_FAST_NAME, TM_GLOBAL_TRAIL_INSTANT_NAME,
    TM_GLOBAL_MISSED_LCANCEL_NAME, TM_GLOBAL_RUN_TURN_NAME, TM_GLOBAL_ACTION_CUES_NAME,
    TM_GLOBAL_INFINITE_SHIELDS_NAME, TM_GLOBAL_INVINCIBILITY_NAME,
};
static const char *color_names[] = { TM_OSD_COLOR_NAMES };
static void hide_tree(JOBJ *joint) {
    joint->flags |= JOBJ_HIDDEN;
    for (JOBJ *child = joint->child; child; child = child->sibling) hide_tree(child);
}
static void hide_gaps(void *data) {
    /* Same column-root/child/sibling layout as native RSS at 0x802364A0.
     * Do not follow a row's next sibling when hiding its subtree. */
    for (unsigned row = 0; row < sizeof(row_ids); ++row) {
        if (row_ids[row] != 255) continue;
        JOBJ *root = *(JOBJ **)((uint8_t *)data + (row < 15 ? 0x2C : 0x34));
        JOBJ *joint = root ? root->child : 0;
        unsigned index = row < 15 ? row : row - 15;
        while (index-- && joint) joint = joint->sibling;
        if (joint) hide_tree(joint);
    }
}

static int flag_for(unsigned id) {
    return TMSettings_NativeFlag(id);
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
        } else {
            Text *text = *(Text **)((uint8_t *)data + (row < 15 ? 0x40 : 0x44));
            Text_SetText(text, (row < 15 ? row : row - 15) + 1, "");
            ((uint8_t *)data)[row + 2] = 0;
        }
    hide_gaps(data);
}

/* Return whether B/Z was consumed, including on unused rows. A/Start still exit,
 * D-pad/stick still navigate and X/Y still choose the screen position. */
int OSD_EditorInput(void *data, unsigned buttons, unsigned row) {
    hide_gaps(data); /* Native initialization can clear the model's hidden flags. */
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
