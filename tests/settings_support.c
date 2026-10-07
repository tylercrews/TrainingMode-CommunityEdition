/* Freestanding memory routines and observability for the PowerPC test image. */
#include "../src/events.h"
#include "../src/ledgedash_logic.h"
#include "../src/osd_context.h"
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
/* Freestanding integer formatting. Native font conversion is executed from
 * the input DOL with the actual assembled converter hook under Unicorn. */
int sprintf(char *out, const char *format, ...) {
    va_list args; va_start(args, format);
    char *start = out;
    while (*format) {
        if (format[0] == '%' && format[1] == 'd') {
            int value = va_arg(args, int); unsigned number;
            if (value < 0) { *out++ = '-'; number = -(unsigned)value; } else number = value;
            char digits[12]; unsigned count = 0;
            do { digits[count++] = '0' + number % 10; number /= 10; } while (number);
            while (count) *out++ = digits[--count];
            format += 2;
        } else *out++ = *format++;
    }
    *out = 0; va_end(args); return out - start;
}
int TestDirty(void) { return stc_memcard_state->memcard_changed; }
int TestTextCopyFormat(char *out, int limit, const char *format, void *args) {
    /* Native SetText fixture inputs are already formatted, as in the timing
     * builder. Keep native conversion, buffer shifting and setters real. */
    char *start = out;
    while (*format) *out++ = *format++;
    *out = 0;
    return out - start;
}
void TestClearDirty(void) { stc_memcard_state->memcard_changed = 0; }
int TestTimingArgument(int tag, int queue, int color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = OSD_ReadMessageTimingArgument(args, tag);
    va_end(args);
    return result;
}

static Text style_text;
static MsgData style_message;
static GOBJ style_object;
static uint32_t style_colors[8];
static float style_scales[8][2], style_positions[8][2];
static char style_strings[8][112];
static int style_count, wait_frame, wait_tag;
static const char *wait_label;
static GOBJ wait_object;
static int wave_frame, wave_hop;
GOBJ *Message_Display(int tag, int queue, int color, char *format, ...) {
    va_list args; va_start(args, format);
    unsigned id = OSD_MessageSettings(tag);
    if (OSD_MessagePointerFirst(tag)) {
        wait_label = va_arg(args, const char *); wait_frame = va_arg(args, int);
    } else {
        wait_frame = va_arg(args, int);
        wait_label = id == 16 ? va_arg(args, const char *) : "Jump Out Of Shine";
        if (id == 0) {
            wave_frame = wait_frame;
            (void)va_arg(args, double);
            (void)va_arg(args, const char *);
            wave_hop = va_arg(args, int);
        }
    }
    va_end(args); wait_tag = tag;
    style_message = (MsgData){.text = &style_text, .settings_id = id, .timing_frame = wait_frame,
        .timing_subtext = OSD_MessageLine(tag), .timing_best = OSD_MessageBestFrame(tag),
        .timing_prefix = -1, .queue_num = queue};
    wait_object.userdata = &style_message;
    style_count = id == 0 ? 3 : 2;
    return &wait_object;
}
int TestWaveFrame(void) { return wave_frame; }
int TestWaveHop(void) { return wave_hop; }
void TestShineBefore(unsigned slot) { OSD_ShineBeforeIASA(Fighter_GetSubcharGObj(slot/2,slot&1)); }
void TestShineAfter(unsigned slot) { OSD_ShineAfterIASA(Fighter_GetSubcharGObj(slot/2,slot&1)); }
void TestStyleTurn(int first_turn, int second_turn) { style_message.timing_turn = first_turn; style_message.timing_second_turn = second_turn; }
int TestStyleEncoded(void) { return style_message.timing_encoded; }

int TestWaitFrame(void) { return wait_frame; }
unsigned TestWaitLabelChar(unsigned i) { return wait_label ? wait_label[i] : 0; }
void TestWaitClear(void) { wait_frame = wait_tag = 0; wait_label = 0; }
void TestWaitDisplay(unsigned slot) { OSD_ActOutWait(Fighter_GetSubcharGObj(slot / 2, slot & 1)); }
int TestWaitTag(void) { return wait_tag; }
void TestContextStep(TMOSDContext *context, unsigned frame, int *sample) {
    TMOSDContext_Step(context, frame, sample[0],sample[1],sample[2],sample[3],sample[4],sample[5],sample[6],sample[7]);
}
void TestContextTick(unsigned slot, unsigned frame) {
    stc_match->time_frames = frame; OSDContext_Tick(Fighter_GetSubcharGObj(slot / 2, slot & 1)->userdata);
}
static unsigned backgrounds;
static Text editor_left, editor_right;
static uint8_t editor_data[0x50];
static const char *editor_choices[29];
static const char *editor_labels[29];
static JOBJ editor_roots[2], editor_joints[29];
static uint32_t editor_colors[29];
static int editor_row(Text *text, int subtext) {
    if (subtext < 1) return -1;
    if (text == &editor_left && subtext <= 15) return subtext - 1;
    if (text == &editor_right && subtext <= 14) return subtext + 14;
    return -1;
}
void Text_SetText(Text *text, int subtext, const char *format, ...) {
    int row = editor_row(text, subtext);
    if (text == &style_text && (unsigned)subtext < 8) {
        /* Capture format and integer args without depending on a host libc. */
        unsigned i = 0; while (format[i] && i < 111) { style_strings[subtext][i] = format[i]; ++i; }
        style_strings[subtext][i] = 0;
    }
    if (row < 0) return;
    if (!format[0]) { editor_choices[row] = ""; editor_labels[row] = ""; return; }
    va_list args;
    va_start(args, format);
    editor_labels[row] = va_arg(args, const char *);
    editor_choices[row] = va_arg(args, const char *);
    va_end(args);
}
void Text_SetScale(Text *text, int subtext, float x, float y) {
    if (text == &style_text && (unsigned)subtext < 8) { style_scales[subtext][0] = x; style_scales[subtext][1] = y; }
}
void Text_SetPosition(Text *text, int subtext, float x, float y) {
    if (text == &style_text && (unsigned)subtext < 8) { style_positions[subtext][0] = x; style_positions[subtext][1] = y; }
}
int Text_AddSubtext(Text *text, float x, float y, char *format, ...) {
    int subtext = style_count++;
    Text_SetPosition(text, subtext, x, y); Text_SetText(text, subtext, format);
    return subtext;
}
void TestStyleFormat(unsigned hitlag, int inline_layout, int y) {
    style_message.timing_hitlag = hitlag; OSD_FormatTiming(&style_message, inline_layout, y);
}
unsigned TestStyleStringChar(unsigned line, unsigned index) { return style_strings[line][index]; }
int TestStyleScale100(unsigned line) { return style_scales[line][0] * 100 + 0.5f; }
int TestStylePrefix(void) { return style_message.timing_prefix; }
int TestStyleTimingLine(void) { return style_message.timing_subtext; }
int TestStyleX(unsigned line) { return style_positions[line][0]; }
int TestStyleY(unsigned line) { return style_positions[line][1]; }
void Text_SetColor(Text *text, int subtext, GXColor *color) {
    int row = editor_row(text, subtext);
    if (row >= 0)
        editor_colors[row] = ((uint32_t)color->r << 24) | ((uint32_t)color->g << 16) |
                             ((uint32_t)color->b << 8) | color->a;
    if ((unsigned)subtext < 8)
        style_colors[subtext] = ((uint32_t)color->r << 24) | ((uint32_t)color->g << 16) |
                                ((uint32_t)color->b << 8) | color->a;
}
void GXLink_Common(GOBJ *gobj, int pass) { backgrounds++; }
void TestStyleInit(int id, int frame, int line, int best) {
    memset(&style_text, 0, sizeof(style_text));
    memset(&style_message, 0, sizeof(style_message));
    memset(style_colors, 0, sizeof(style_colors));
    memset(style_scales, 0, sizeof(style_scales)); memset(style_strings, 0, sizeof(style_strings));
    style_count = 3; style_message.timing_prefix = -1;
    backgrounds = 0;
    style_message.text = &style_text;
    style_message.settings_id = id;
    style_message.timing_frame = frame;
    style_message.timing_subtext = line;
    style_message.timing_best = best;
    style_object.userdata = &style_message;
}
void TestStyleCallerColor(int subtext, uint32_t color) { style_colors[subtext] = color; }
void TestStyleQueue(int queue) { style_message.queue_num = queue; }
void TestStyleDraw(void) { OSD_MessageGX(&style_object, 2); }
uint32_t TestStyleColor(int subtext) { return style_colors[subtext]; }
int TestStyleHidden(void) { return style_text.hidden; }
unsigned TestStyleBackgrounds(void) { return backgrounds; }
void TestEditorInit(void) {
    memset(editor_data, 0, sizeof(editor_data));
    memset(editor_choices, 0, sizeof(editor_choices));
    memset(editor_joints, 0, sizeof(editor_joints));
    memset(editor_roots, 0, sizeof(editor_roots));
    editor_roots[0].child = &editor_joints[0];
    editor_roots[1].child = &editor_joints[15];
    for (int i = 0; i < 28; ++i)
        if (i != 14) editor_joints[i].sibling = &editor_joints[i + 1];
    *(JOBJ **)(editor_data + 0x2C) = &editor_roots[0];
    *(JOBJ **)(editor_data + 0x34) = &editor_roots[1];
    *(Text **)(editor_data + 0x40) = &editor_left;
    *(Text **)(editor_data + 0x44) = &editor_right;
    OSD_EditorInit(editor_data);
}
int TestEditorInput(unsigned buttons, unsigned row) { return OSD_EditorInput(editor_data, buttons, row); }
unsigned TestEditorCache(unsigned row) { return editor_data[row + 2]; }
void TestEditorAnimate(unsigned row) { editor_data[row + 2] = *(volatile uint8_t *)0x804A04F4; }
unsigned TestEditorChoiceChar(unsigned row, unsigned offset) { return editor_choices[row][offset]; }
uint32_t TestEditorColor(unsigned row) { return editor_colors[row]; }
unsigned TestEditorLabelChar(unsigned row, unsigned offset) { return editor_labels[row][offset]; }
unsigned TestEditorHidden(unsigned row) { return !!(editor_joints[row].flags & JOBJ_HIDDEN); }

static GOBJ cue_manager, cue_objects[12];
static FighterData cue_data[12];
static ftCommonData cue_common;
static Playerblock cue_players[6];
static int cue_active[12];
static float cue_lengths[12];
static void (*cue_early)(GOBJ *), (*cue_late)(GOBJ *);
static uint32_t cue_draw_colors[12];
static unsigned cue_draw_flags[12];
static unsigned cue_draw_count[12];
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
Playerblock *Fighter_GetPlayerblock(int ply) { return ply >= 0 && ply < 6 ? &cue_players[ply] : 0; }
int Fighter_GetIntangibleFrames(GOBJ *gobj) {
    FighterData *data = gobj->userdata;
    int status = (*(u8 *)((u8 *)data + 0x221D) & 2) ? 1 : 0;
    if (data->hurt.kind_script > status) status = data->hurt.kind_script;
    if (data->hurt.kind_game > status) status = data->hurt.kind_game;
    return status;
}
float Fighter_GetCurrentAnimLength(GOBJ *gobj) { return cue_lengths[gobj - cue_objects]; }
static void TestCueNativeDraw(GOBJ *gobj, int pass) {
    unsigned slot = gobj - cue_objects;
    cue_draw_count[slot]++;
    FighterData *data = gobj->userdata;
    /* Execute the actual native chooser (installed by the PPC test machine),
     * rather than assuming the slot that the adapter is supposed to select. */
    ColorOverlay *selected = ((ColorOverlay *(*)(FighterData *))0x800C0658)(data);
    GXColor color = selected->hex;
    cue_draw_colors[slot] = ((u32)color.r << 24) | ((u32)color.g << 16) | ((u32)color.b << 8) | color.a;
    cue_draw_flags[slot] = (data->color[0].color_enable << 2) | (data->color[1].color_enable << 1) | data->color[2].color_enable;
}
void TestCueInit(void) {
    ActionCues_SceneChange();
    memset(cue_data, 0, sizeof(cue_data));
    memset(cue_objects, 0, sizeof(cue_objects));
    memset(cue_active, 0, sizeof(cue_active));
    memset(cue_draw_count, 0, sizeof(cue_draw_count));
    memset(cue_players, 0, sizeof(cue_players));
    *stc_ftcommon = &cue_common;
    cue_common.lcancel_input_window = 7; cue_common.x260 = 60;
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
void TestCueFrozen(unsigned slot, int frozen) { cue_data[slot].flags.hitlag = frozen; cue_data[slot].flags.freeze = frozen; }
void TestCueIASA(unsigned slot, int ready) { cue_data[slot].flags.past_iasa = ready; }
void TestCueKind(unsigned slot, int kind) { cue_data[slot].kind = kind; }
void TestCueSpawn(unsigned slot, int spawn, int dead) {
    cue_data[slot].spawn_num = spawn; cue_data[slot].flags.dead = dead;
}
void TestCueInputTimer(unsigned slot, int timer) { cue_data[slot].input.timer_trigger_any_ignore_hitlag = timer; }
unsigned TestCueDrawCount(unsigned slot) { return cue_draw_count[slot]; }
int TestCueWrapped(unsigned slot) { return cue_objects[slot].gx_cb != TestCueNativeDraw; }
void TestCueLocal(unsigned slot, unsigned rgba) {
    GXColor color = {rgba >> 24, rgba >> 16, rgba >> 8, rgba};
    ActionCues_SetLocalOverlay(&cue_objects[slot], rgba ? &color : 0);
}
void TestCueProtection(unsigned slot, int script, int game, int armor100) {
    cue_data[slot].hurt.kind_script = script; cue_data[slot].hurt.kind_game = game;
    cue_data[slot].dmg.armor = armor100 / 100.0f;
}
void TestCueCapsules(unsigned slot, int num, int state) {
    cue_data[slot].hurt_num = num;
    for (unsigned i = 0; i < countof(cue_data[slot].hurtbox); ++i) cue_data[slot].hurtbox[i].state = state;
}
void TestCueCapsule(unsigned slot, unsigned index, int state) { cue_data[slot].hurtbox[index].state = state; }
void TestCuePlayer(unsigned ply, int accent, int cpu) {
    cue_players[ply].color_accent = accent; cue_players[ply].p_kind = cpu;
}
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
int TestLdshSurface(LdshSurface *surface, int desired100, int ledge100, int platform, float *result) {
    return LdshSurface_Target(surface, desired100 / 100.f, ledge100 / 100.f, platform, &result[0], &result[1]);
}
