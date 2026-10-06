#include "osd_style.h"
#include "settings.h"

static int tagged(int kind) { return ((uint32_t)kind & 0xC0000000u) == 0x40000000u; }
uint32_t OSD_MessageTag(int kind, unsigned id, unsigned argument, unsigned line, unsigned inline_layout) {
    if (id == 64) id = 8;
    if (id >= 32 || argument > 3 || line > 2) return (uint32_t)kind;
    return 0x40000000u | ((uint32_t)kind & 255) | (id << 8) |
           (argument << 16) | (line << 19) | (!!inline_layout << 21);
}
int OSD_MessageKind(int kind) {
    if (!tagged(kind)) return kind;
    return (kind & 255) == 255 ? -1 : kind & 255;
}
int OSD_MessageSettings(int kind) {
    if (!tagged(kind)) return -1;
    unsigned id = ((uint32_t)kind >> 8) & 31;
    for (unsigned i = 0; i < TM_SETTINGS_OSDS; ++i)
        if (TMSettings_OSDIDs[i] == id) return id;
    return -1;
}
unsigned OSD_MessageArgument(int kind) { return tagged(kind) ? ((uint32_t)kind >> 16) & 7 : 0; }
unsigned OSD_MessageLine(int kind) { return tagged(kind) ? ((uint32_t)kind >> 19) & 3 : 0; }
unsigned OSD_MessageInline(int kind) { return tagged(kind) ? ((uint32_t)kind >> 21) & 1 : 0; }
unsigned OSD_MessagePointerFirst(int kind) { return tagged(kind) ? ((uint32_t)kind >> 22) & 1 : 0; }
unsigned OSD_MessageBestFrame(int kind) { return tagged(kind) ? (((uint32_t)kind >> 23) & 31) + 1 : 1; }
int OSD_SameReplacement(int kind_a, int settings_a, int kind_b, int settings_b) {
    return kind_a != -1 && kind_a == kind_b && settings_a == settings_b;
}
uint32_t OSD_PaletteColor(unsigned choice) {
    static const uint32_t palette[] = {
        0x00000000, 0xFFFFFFFF, 0xFF4646FF, 0x8DFF6EFF,
        0x4691FFFF, 0xFFF000FF, 0x00FFFFFF, 0xFF50FFFF,
    };
    return choice < 8 ? palette[choice] : palette[1];
}
uint32_t OSD_TimingColor(int frame) {
    return OSD_TimingColorFor(frame, 1);
}
uint32_t OSD_TimingColorFor(int frame, unsigned best) {
    if (best < 1 || best > 32) best = 1;
    if (frame == (int)best) return 0x00FFFFFF;
    if (frame == (int)best + 1) return 0x8DFF6EFF;
    if (frame == (int)best + 2) return 0xFFF000FF;
    return 0xFFA2BAFF;
}
uint32_t OSD_WavedashHopColor(int short_hop, int frame) {
    return short_hop == 1 ? (frame == 1 ? 0x00FFFFFF : 0x8DFF6EFF) : 0xFFA2BAFF;
}
