#include "events.h"
#include "settings.h"
#include <stddef.h>

typedef char settings_mask_offset[(offsetof(Memcard, TM_OSDEnabled) == TM_SETTINGS_OFFSET) ? 1 : -1];
typedef char settings_extra_offset[(offsetof(Memcard, TM_SettingsExtraFlags) == 0x1F4C) ? 1 : -1];
typedef char settings_tail_offset[(offsetof(Memcard, TM_SettingsReserved) == 0x1F4D) ? 1 : -1];
typedef char settings_boundary[(offsetof(Memcard, unk2004) == TM_SETTINGS_OFFSET + TM_SETTINGS_SIZE) ? 1 : -1];

static u8 last_record[TM_SETTINGS_SIZE];
static u8 fallback[TM_SETTINGS_SIZE];
static int seen;
static int last_owns;
static int status;

static int same_bytes(const u8 *a, const u8 *b, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        if (a[i] != b[i]) return 0;
    return 1;
}

static u8 *settings_view(void) {
    u8 *record = (u8 *)stc_memcard + TM_SETTINGS_OFFSET;
    int owns = same_bytes((const u8 *)0x80000000, (const u8 *)TM_GAME_ID, 6);
    if (!seen || owns != last_owns || !same_bytes(record, last_record, TM_SETTINGS_SIZE)) {
        status = TMSettings_Prepare(record, owns);
        if (status == TM_SETTINGS_UNSUPPORTED || status == TM_SETTINGS_FOREIGN)
            TMSettings_Init(fallback);
        else if (status == TM_SETTINGS_MIGRATED || status == TM_SETTINGS_REPAIRED)
            stc_memcard_state->memcard_changed = true;
        memcpy(last_record, record, TM_SETTINGS_SIZE);
        seen = 1;
        last_owns = owns;
    }
    return status == TM_SETTINGS_UNSUPPORTED || status == TM_SETTINGS_FOREIGN ? fallback : record;
}

uint32_t Settings_Get(unsigned field, unsigned index) {
    return TMSettings_Read(settings_view(), field, index);
}

void Settings_Set(unsigned field, unsigned index, uint32_t value) {
    u8 *view = settings_view();
    if (TMSettings_Write(view, field, index, value) && view != fallback) {
        stc_memcard_state->memcard_changed = true;
        memcpy(last_record, view, TM_SETTINGS_SIZE);
    }
}

int Settings_Status(void) {
    settings_view();
    return status;
}
