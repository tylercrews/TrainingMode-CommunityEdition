#include "events.h"
#include "settings.h"
#include <stddef.h>

typedef char settings_mask_offset[(offsetof(Memcard, TM_OSDEnabled) == TM_SETTINGS_OFFSET) ? 1 : -1];
typedef char settings_extra_offset[(offsetof(Memcard, TM_SettingsExtraFlags) == 0x1F4C) ? 1 : -1];
typedef char settings_tail_offset[(offsetof(Memcard, TM_SettingsReserved) == 0x1F4D) ? 1 : -1];
typedef char settings_boundary[(offsetof(Memcard, unk2004) == TM_SETTINGS_OFFSET + TM_SETTINGS_SIZE) ? 1 : -1];
typedef char card_dirty_word[(sizeof(((MemcardState *)0)->memcard_changed) == 4 && offsetof(MemcardState, memcard_changed) == 0xC) ? 1 : -1];
typedef char card_enable_word[(sizeof(((MemcardState *)0)->enable) == 4 && offsetof(MemcardState, enable) == 0x18 && offsetof(MemcardState, x5C) == 0x5C) ? 1 : -1];

static u8 last_record[TM_SETTINGS_SIZE];
static u8 fallback[TM_SETTINGS_SIZE];
static int seen;
static int last_owns;
static int status;
static unsigned osd_page;
static int pending_save; /* Survives native archive unload clearing its own xC. */
static int queued_to_native;
static int card_ready(void) {
    return stc_memcard_state->enable && stc_memcard_state->x5C &&
        stc_memcard_work->work_area && stc_memcard_work->buffer && stc_memcard_state->x8 == 0;
}
static void mark_changed(void) {
    pending_save = 1;
    queued_to_native = 0;
    if (card_ready()) { stc_memcard_state->memcard_changed = true; queued_to_native = 1; }
}

/* Keep pointers as runtime arguments. Inlining this against low-memory identity
 * can make GCC emit a symbol-minus-0x80000000 relocation; the MEX loader does
 * not preserve that addend and the resulting indexed read wraps into low RAM. */
__attribute__((noinline)) static int same_bytes(const u8 *a, const u8 *b, unsigned count) {
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
        else if (status == TM_SETTINGS_MIGRATED || status == TM_SETTINGS_REPAIRED) {
            mark_changed();
        }
        memcpy(last_record, record, TM_SETTINGS_SIZE);
        seen = 1;
        last_owns = owns;
    }
    return status == TM_SETTINGS_UNSUPPORTED || status == TM_SETTINGS_FOREIGN ? fallback : record;
}

uint32_t Settings_Get(unsigned field, unsigned index) {
    if (field == TM_SETTING_OSD_PAGE) return osd_page;
    return TMSettings_Read(settings_view(), field, index);
}

void Settings_Set(unsigned field, unsigned index, uint32_t value) {
    if (field == TM_SETTING_OSD_PAGE) { if (value < 19) osd_page = value; return; }
    u8 *view = settings_view();
    if (TMSettings_Write(view, field, index, value) && view != fallback) {
        mark_changed();
        memcpy(last_record, view, TM_SETTINGS_SIZE);
    }
}

int Settings_Status(void) {
    settings_view();
    return status;
}

void Settings_CommitPending(void) {
    if (!pending_save) return;
    if (!same_bytes((const u8 *)0x80000000, (const u8 *)TM_GAME_ID, 6)) return;
    if (!card_ready()) return;
    /* Native per-frame autosave owns requests, polling, checksums and errors.
     * Requeue after returning to ESS, even if archive teardown reset dirty. */
    if (queued_to_native && stc_memcard_state->x10 == 1 && !stc_memcard_state->memcard_changed) {
        pending_save = 0; /* Native request has accepted the current data. */
        return;
    }
    stc_memcard_state->memcard_changed = true;
    queued_to_native = 1;
}
