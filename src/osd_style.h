#ifndef TM_OSD_STYLE_H
#define TM_OSD_STYLE_H
#include <stdint.h>
#include <stdarg.h>

#define TM_OSD_COLOR_NAMES "Off", "White", "Red", "Green", "Blue", "Yellow", "Cyan", "Magenta"
#define TM_OSD_POINTER_FIRST (1u << 22)
/* Tagged message kind: low byte is the unchanged queue/dedup kind.
 * Bits 8..12 identify the settings category; 16..18 select an integer vararg;
 * 19..20 select its text line; 21 requests the Wavedash inline layout;
 * Bit 22 declares a leading string pointer before the integer timing argument;
 * 23..27 encode best-frame minus one (default best frame is 1).
 * The top-bit pattern 01 distinguishes tags from legacy -1 messages.
 */
uint32_t OSD_MessageTag(int kind, unsigned id, unsigned argument, unsigned line, unsigned inline_layout);
int OSD_MessageKind(int tagged_kind);
int OSD_MessageSettings(int tagged_kind);
unsigned OSD_MessageArgument(int tagged_kind);
unsigned OSD_MessageLine(int tagged_kind);
unsigned OSD_MessageInline(int tagged_kind);
unsigned OSD_MessagePointerFirst(int tagged_kind);
unsigned OSD_MessageBestFrame(int tagged_kind);
int OSD_SameReplacement(int kind_a, int settings_a, int kind_b, int settings_b);
uint32_t OSD_PaletteColor(unsigned choice);
uint32_t OSD_TimingColor(int displayed_frame);
uint32_t OSD_TimingColorFor(int displayed_frame, unsigned best_frame);
uint32_t OSD_WavedashHopColor(int short_hop, int displayed_frame);
static inline int OSD_ReadTimingArgument(va_list args, unsigned index) {
    if (!index || index > 3) return -1;
    va_list copy;
    va_copy(copy, args);
    int frame = -1;
    for (unsigned i = 0; i < index; ++i) frame = va_arg(copy, int);
    va_end(copy);
    return frame;
}
static inline int OSD_ReadMessageTimingArgument(va_list args, int tag) {
    unsigned index = OSD_MessageArgument(tag);
    if (!OSD_MessagePointerFirst(tag)) return OSD_ReadTimingArgument(args, index);
    if (index < 2 || index > 3) return -1;
    va_list copy; va_copy(copy, args);
    (void)va_arg(copy, const char *);
    int frame = OSD_ReadTimingArgument(copy, index - 1);
    va_end(copy);
    return frame;
}
#endif
