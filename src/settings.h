#ifndef TM_SETTINGS_H
#define TM_SETTINGS_H

#include <stdint.h>

/* Serialized bytes, not a C struct/bitfield ABI. All offsets are within 0x1F24..0x1F4F. */
#define TM_SETTINGS_OFFSET 0x1F24
#define TM_SETTINGS_SIZE 44
#define TM_SETTINGS_VERSION 1
#define TM_SETTINGS_OVERLAYS 18
#define TM_SETTINGS_LEGACY_OVERLAYS 17
#define TM_SETTINGS_OVERLAY_CHOICES 11
#define TM_SETTINGS_OSDS 19
#define TM_SETTINGS_FLAGS_OFFSET 10
#define TM_SETTINGS_OVERLAYS_OFFSET 12
#define TM_SETTINGS_COLORS_OFFSET 30
#define TM_SETTINGS_SIGNATURE_OFFSET 38
#define TM_SETTINGS_RESERVED_OFFSET 40

/* Shared labels and native L-menu row bindings. Build-generated ASM uses these names too. */
#define TM_GLOBAL_TRAIL_VERY_FAST_ROW 2
#define TM_GLOBAL_TRAIL_INSTANT_ROW 4
#define TM_GLOBAL_OSDS_OFF_ROW 6
#define TM_GLOBAL_OSDS_OFF_NAME "TURN OSDS OFF"
#define TM_GLOBAL_TRAIL_VERY_FAST_NAME "Hitbox Trails Very Fast"
#define TM_GLOBAL_TRAIL_INSTANT_NAME "Hitbox Trails Instant"
#define TM_GLOBAL_TRAIL_STATE_NAMES "Global: Off", "Global: Very Fast", "Global: Instant", "Global: Both On"

enum TMSettingsStatus {
    TM_SETTINGS_READY,
    TM_SETTINGS_MIGRATED,
    TM_SETTINGS_REPAIRED,
    TM_SETTINGS_UNSUPPORTED,
    TM_SETTINGS_FOREIGN,
};

enum TMSettingsColor {
    TM_COLOR_OFF, TM_COLOR_WHITE, TM_COLOR_RED, TM_COLOR_GREEN,
    TM_COLOR_BLUE, TM_COLOR_YELLOW, TM_COLOR_CYAN, TM_COLOR_MAGENTA,
};

enum TMSettingsFlag {
    TM_FLAG_OSDS_OFF, TM_FLAG_TRAILS_VERY_FAST, TM_FLAG_TRAILS_INSTANT,
    TM_FLAG_MISSED_LCANCEL, TM_FLAG_LAST_BLOCKED_FRAME, TM_FLAG_INFINITE_SHIELDS,
    TM_FLAG_COUNT,
};

/* These field numbers are mirrored in ASM/Globals.s. Append; never reorder. */
enum TMSettingsField {
    TM_SETTING_OSD_MASK, TM_SETTING_OSD_POSITION, TM_SETTING_EVENT_PAGE,
    TM_SETTING_RECOMMENDED, TM_SETTING_ADVANCE, TM_SETTING_DECREMENT,
    TM_SETTING_DPAD_UP, TM_SETTING_DPAD_DOWN, TM_SETTING_DPAD_LEFT,
    TM_SETTING_DPAD_RIGHT, TM_SETTING_INPUT_DISPLAY, TM_SETTING_FLAG,
    TM_SETTING_OVERLAY_HMN, TM_SETTING_OVERLAY_CPU, TM_SETTING_OSD_COLOR,
    TM_SETTING_OSD_ENABLED,
    TM_SETTING_NATIVE_ROW,
};

extern const uint8_t TMSettings_OSDIDs[TM_SETTINGS_OSDS];
void TMSettings_Init(uint8_t record[TM_SETTINGS_SIZE]);
int TMSettings_Prepare(uint8_t record[TM_SETTINGS_SIZE], int owns_save);
uint32_t TMSettings_Read(const uint8_t record[TM_SETTINGS_SIZE], unsigned field, unsigned index);
int TMSettings_Write(uint8_t record[TM_SETTINGS_SIZE], unsigned field, unsigned index, uint32_t value);

/* One runtime service in eventMenu.dat; event modules call it through EventVars. */
typedef struct TMSettingsAPI {
    uint32_t (*get)(unsigned field, unsigned index);
    void (*set)(unsigned field, unsigned index, uint32_t value);
    int (*status)(void);
} TMSettingsAPI;

uint32_t Settings_Get(unsigned field, unsigned index);
void Settings_Set(unsigned field, unsigned index, uint32_t value);
int Settings_Status(void);

#endif
