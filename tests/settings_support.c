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
