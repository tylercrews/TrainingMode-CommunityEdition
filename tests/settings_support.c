/* Freestanding memory routines and observability for the PowerPC test image. */
#include "../src/events.h"
#include <stddef.h>

void memcpy(void *dst, const void *src, int size) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (size--) *d++ = *s++;
}
void memset(void *dst, int value, int size) {
    unsigned char *d = dst;
    while (size--) *d++ = value;
}
int TestDirty(void) { return stc_memcard_state->memcard_changed; }
void TestClearDirty(void) { stc_memcard_state->memcard_changed = 0; }
int TestTimingArgument(int tag, int queue, int color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = OSD_ReadTimingArgument(args, OSD_MessageArgument(tag));
    va_end(args);
    return result;
}

static Text style_text;
static MsgData style_message;
static GOBJ style_object;
static uint32_t style_colors[4];
static unsigned backgrounds;
static Text editor_left, editor_right;
static uint8_t editor_data[0x50];
static const char *editor_choices[29];
static uint32_t editor_colors[29];
static int editor_row(Text *text, int subtext) {
    if (subtext < 1) return -1;
    if (text == &editor_left && subtext <= 15) return subtext - 1;
    if (text == &editor_right && subtext <= 14) return subtext + 14;
    return -1;
}
void Text_SetText(Text *text, int subtext, const char *format, ...) {
    int row = editor_row(text, subtext);
    if (row < 0) return;
    va_list args;
    va_start(args, format);
    va_arg(args, const char *); /* label */
    editor_choices[row] = va_arg(args, const char *);
    va_end(args);
}
void Text_SetScale(Text *text, int subtext, float x, float y) {}
void Text_SetColor(Text *text, int subtext, GXColor *color) {
    int row = editor_row(text, subtext);
    if (row >= 0)
        editor_colors[row] = ((uint32_t)color->r << 24) | ((uint32_t)color->g << 16) |
                             ((uint32_t)color->b << 8) | color->a;
    if ((unsigned)subtext < 4)
        style_colors[subtext] = ((uint32_t)color->r << 24) | ((uint32_t)color->g << 16) |
                                ((uint32_t)color->b << 8) | color->a;
}
void GXLink_Common(GOBJ *gobj, int pass) { backgrounds++; }
void TestStyleInit(int id, int frame, int line, int best) {
    memset(&style_text, 0, sizeof(style_text));
    memset(&style_message, 0, sizeof(style_message));
    memset(style_colors, 0, sizeof(style_colors));
    backgrounds = 0;
    style_message.text = &style_text;
    style_message.settings_id = id;
    style_message.timing_frame = frame;
    style_message.timing_subtext = line;
    style_message.timing_best = best;
    style_object.userdata = &style_message;
}
void TestStyleCallerColor(int subtext, uint32_t color) { style_colors[subtext] = color; }
void TestStyleDraw(void) { OSD_MessageGX(&style_object, 2); }
uint32_t TestStyleColor(int subtext) { return style_colors[subtext]; }
int TestStyleHidden(void) { return style_text.hidden; }
unsigned TestStyleBackgrounds(void) { return backgrounds; }
void TestEditorInit(void) {
    memset(editor_data, 0, sizeof(editor_data));
    memset(editor_choices, 0, sizeof(editor_choices));
    *(Text **)(editor_data + 0x40) = &editor_left;
    *(Text **)(editor_data + 0x44) = &editor_right;
    OSD_EditorInit(editor_data);
}
int TestEditorInput(unsigned buttons, unsigned row) { return OSD_EditorInput(editor_data, buttons, row); }
unsigned TestEditorCache(unsigned row) { return editor_data[row + 2]; }
void TestEditorAnimate(unsigned row) { editor_data[row + 2] = *(volatile uint8_t *)0x804A04F4; }
unsigned TestEditorChoiceChar(unsigned row, unsigned offset) { return editor_choices[row][offset]; }
uint32_t TestEditorColor(unsigned row) { return editor_colors[row]; }

static GOBJ cue_manager, cue_objects[12];
static FighterData cue_data[12];
static ftCommonData cue_common;
static int cue_active[12];
static float cue_lengths[12];
static void (*cue_early)(GOBJ *), (*cue_late)(GOBJ *);
static uint32_t cue_draw_colors[12];
static unsigned cue_draw_flags[12];
GOBJ *GObj_Create(int entity_class, int p_link, int p_priority) { return &cue_manager; }
GOBJProc *GObj_AddProc(GOBJ *gobj, void *callback, int priority) {
    if (priority == 0) cue_early = callback;
    if (priority == 23) cue_late = callback;
    return 0;
}
GOBJ *Fighter_GetSubcharGObj(int ply, int sub) {
    unsigned slot = ply * 2 + sub;
    return slot < 12 && cue_active[slot] ? &cue_objects[slot] : 0;
}
float Fighter_GetCurrentAnimLength(GOBJ *gobj) { return cue_lengths[gobj - cue_objects]; }
static void TestCueNativeDraw(GOBJ *gobj, int pass) {
    unsigned slot = gobj - cue_objects;
    FighterData *data = gobj->userdata;
    GXColor color = data->color[1].hex;
    cue_draw_colors[slot] = ((u32)color.r << 24) | ((u32)color.g << 16) | ((u32)color.b << 8) | color.a;
    cue_draw_flags[slot] = (data->color[0].color_enable << 2) | (data->color[1].color_enable << 1) | data->color[2].color_enable;
}
void TestCueInit(void) {
    ActionCues_SceneChange();
    memset(cue_data, 0, sizeof(cue_data));
    memset(cue_objects, 0, sizeof(cue_objects));
    memset(cue_active, 0, sizeof(cue_active));
    *stc_ftcommon = &cue_common;
    cue_common.xe4 = 7; cue_common.x260 = 60;
    for (unsigned slot = 0; slot < 12; ++slot) {
        cue_objects[slot].userdata = &cue_data[slot];
        cue_objects[slot].gx_cb = TestCueNativeDraw;
        cue_data[slot].ply = slot / 2; cue_data[slot].flags.ms = slot & 1;
        cue_data[slot].state_id = ASID_WAIT;
        cue_data[slot].state.rate = 1;
        cue_lengths[slot] = 40;
    }
    stc_match->time_frames = 0;
    ActionCues_MatchStart();
}
uintptr_t TestCueData(unsigned slot) { return (uintptr_t)&cue_data[slot]; }
uintptr_t TestCueObject(unsigned slot) { return (uintptr_t)&cue_objects[slot]; }
void TestCueState(unsigned slot, int state, int frame, int end, int rate100) {
    cue_active[slot] = 1;
    cue_data[slot].state_id = state;
    cue_data[slot].state.frame = frame;
    cue_data[slot].state.rate = rate100 / 100.0f;
    cue_lengths[slot] = end;
}
void TestCueLanding(unsigned slot, int lag, int allow_interrupt) {
    cue_data[slot].attr.normal_landing_lag = lag;
    cue_data[slot].state_var.state_var1 = allow_interrupt;
}
void TestCueFrozen(unsigned slot, int frozen) { cue_data[slot].flags.hitlag = frozen; }
void TestCueIASA(unsigned slot, int ready) { cue_data[slot].flags.past_iasa = ready; }
void TestCueKind(unsigned slot, int kind) { cue_data[slot].kind = kind; }
void TestCueMissed(unsigned slot, int timer) {
    cue_data[slot].input.timer_trigger_any_ignore_hitlag = timer;
    ActionCues_LCancel(&cue_objects[slot]);
}
void TestCueTick(unsigned frame) {
    stc_match->time_frames = frame;
    cue_early(&cue_manager);
    cue_late(&cue_manager);
}
void TestCueDraw(unsigned slot) { cue_objects[slot].gx_cb(&cue_objects[slot], 2); }
uint32_t TestCueDrawColor(unsigned slot) { return cue_draw_colors[slot]; }
unsigned TestCueDrawFlags(unsigned slot) { return cue_draw_flags[slot]; }
int TestCueRemaining(int frame100, int rate100, int boundary100) {
    return TMCue_Remaining(frame100 / 100.0f, rate100 / 100.0f, boundary100 / 100.0f);
}
int TestCueScriptIASA(TMCueScript *script, int rate100) {
    return TMCue_ScriptIASA(script, rate100 / 100.0f);
}
void TestCueScript(unsigned slot, const uint32_t *script, int timer100, int frame100) {
    cue_data[slot].script.script_current = (int *)script;
    cue_data[slot].script.script_event_timer = timer100 / 100.0f;
    cue_data[slot].script.script_frame_timer = frame100 / 100.0f;
}
