#include "settings.h"
#include <string.h>

/* Stable packed slots, independent of sparse native OSD IDs and message kinds. */
const uint8_t TMSettings_OSDIDs[TM_SETTINGS_OSDS] = {
    0, 1, 3, 5, 8, 9, 10, 12, 13, 14, 16, 18, 19, 20, 21, 22, 24, 26, 28,
};
int TMSettings_NativeFlag(unsigned row) {
    switch (row) {
    case TM_GLOBAL_OSDS_OFF_ROW: return TM_FLAG_OSDS_OFF;
    case TM_GLOBAL_TRAIL_VERY_FAST_ROW: return TM_FLAG_TRAILS_VERY_FAST;
    case TM_GLOBAL_TRAIL_INSTANT_ROW: return TM_FLAG_TRAILS_INSTANT;
    case TM_GLOBAL_MISSED_LCANCEL_ROW: return TM_FLAG_MISSED_LCANCEL;
    case TM_GLOBAL_ACTION_CUES_ROW: return TM_FLAG_LAST_BLOCKED_FRAME;
    case TM_GLOBAL_RUN_TURN_ROW: return TM_FLAG_RUN_TURNAROUND;
    case TM_GLOBAL_INFINITE_SHIELDS_ROW: return TM_FLAG_INFINITE_SHIELDS;
    case TM_GLOBAL_INVINCIBILITY_ROW: return TM_FLAG_INVINCIBILITY;
    default: return -1;
    }
}

static uint32_t read_mask(const uint8_t *r) {
    return ((uint32_t)r[0] << 24) | ((uint32_t)r[1] << 16) | ((uint32_t)r[2] << 8) | r[3];
}
static void write_mask(uint8_t *r, uint32_t mask) {
    r[0] = mask >> 24; r[1] = mask >> 16; r[2] = mask >> 8; r[3] = mask;
}
static int bytes_differ(const uint8_t *a, const uint8_t *b) {
    for (unsigned i = 0; i < TM_SETTINGS_SIZE; ++i)
        if (a[i] != b[i]) return 1;
    return 0;
}
static int osd_slot(unsigned id) {
    for (unsigned i = 0; i < TM_SETTINGS_OSDS; ++i)
        if (TMSettings_OSDIDs[i] == id) return i;
    return -1;
}
static unsigned read_color(const uint8_t *r, unsigned slot) {
    unsigned bit = slot * 3, byte = TM_SETTINGS_COLORS_OFFSET + bit / 8, shift = bit % 8;
    unsigned bits = r[byte];
    if (shift > 5) bits |= (unsigned)r[byte + 1] << 8;
    return (bits >> shift) & 7;
}
static void write_color(uint8_t *r, unsigned slot, unsigned color) {
    unsigned bit = slot * 3, byte = TM_SETTINGS_COLORS_OFFSET + bit / 8, shift = bit % 8;
    unsigned bits = r[byte];
    if (shift > 5) bits |= (unsigned)r[byte + 1] << 8;
    bits = (bits & ~(7u << shift)) | (color << shift);
    r[byte] = bits;
    if (shift > 5) r[byte + 1] = bits >> 8;
}

void TMSettings_Init(uint8_t r[TM_SETTINGS_SIZE]) {
    memset(r, 0, TM_SETTINGS_SIZE);
    r[4] = 1; /* Sides */
    r[5] = 1; /* General Tech */
    r[TM_SETTINGS_FLAGS_OFFSET] = TM_SETTINGS_VERSION << 6;
    r[TM_SETTINGS_SIGNATURE_OFFSET] = 'T';
    r[TM_SETTINGS_SIGNATURE_OFFSET + 1] = 'Y';
}

static void validate_prefix(uint8_t *r) {
    if (r[4] >= 4) r[4] = 1;
    if (r[5] >= 3) r[5] = 1;
    if (r[6] >= 2) r[6] = 0;
    if ((r[7] & 15) >= 5) r[7] &= 0xF0;
    if ((r[7] >> 4) >= 6) r[7] &= 0x0F;
    if ((r[8] & 15) >= 2) r[8] &= 0xF0;
    if ((r[8] >> 4) >= 3) r[8] &= 0x0F;
    if ((r[9] & 15) >= 2) r[9] &= 0xF0;
    if ((r[9] >> 4) >= 2) r[9] &= 0x0F;
    if (r[11] >= 4) r[11] = 0;
}

int TMSettings_Prepare(uint8_t r[TM_SETTINGS_SIZE], int owns_save) {
    if (!owns_save) return TM_SETTINGS_FOREIGN;
    int signed_format = r[38] == 'T' && r[39] == 'Y';
    unsigned old_version = r[10] >> 6;
    if (signed_format && old_version != 1 && old_version != 2 && old_version != TM_SETTINGS_VERSION)
        return TM_SETTINGS_UNSUPPORTED; /* Never reinterpret or overwrite a future format. */

    uint8_t before[TM_SETTINGS_SIZE];
    memcpy(before, r, sizeof(before));
    if (signed_format && old_version < TM_SETTINGS_VERSION) {
        r[10] = (r[10] & 0x3F) | (TM_SETTINGS_VERSION << 6);
        /* Version 1 owns neither extension flag; version 2 already owns TurnRun. */
        r[TM_SETTINGS_EXTRA_FLAGS_OFFSET] &= ~(old_version == 1 ? 3u : 2u);
    }
    if (!signed_format) {
        /* Read both old lists before overwriting any of their overlapping bytes. */
        uint8_t legacy[32];
        memcpy(legacy, r + 12, sizeof(legacy));
        memset(r + 12, 0, 32);
        r[10] = TM_SETTINGS_VERSION << 6; /* All new global flags default Off. */
        for (unsigned actor = 0; actor < 2; ++actor) {
            for (unsigned pair = 0; pair < 8; ++pair) {
                unsigned p = actor * 16 + pair * 2;
                unsigned group = legacy[p], choice = legacy[p + 1];
                if (group < TM_SETTINGS_LEGACY_OVERLAYS && choice < TM_SETTINGS_OVERLAY_CHOICES && choice) {
                    unsigned shift = actor * 4;
                    r[12 + group] = (r[12 + group] & ~(15u << shift)) | (choice << shift);
                }
            }
        }
    }
    validate_prefix(r);
    for (unsigned group = 0; group < TM_SETTINGS_OVERLAYS; ++group) {
        if ((r[12 + group] & 15) >= TM_SETTINGS_OVERLAY_CHOICES) r[12 + group] &= 0xF0;
        if ((r[12 + group] >> 4) >= TM_SETTINGS_OVERLAY_CHOICES) r[12 + group] &= 0x0F;
    }
    /* The unchanged enable mask remains authoritative for legacy native readers. */
    uint32_t mask = read_mask(r);
    for (unsigned slot = 0; slot < TM_SETTINGS_OSDS; ++slot) {
        unsigned color = read_color(r, slot);
        if (!(mask & (1u << TMSettings_OSDIDs[slot]))) color = TM_COLOR_OFF;
        else if (color == TM_COLOR_OFF) color = TM_COLOR_WHITE;
        write_color(r, slot, color);
    }
    /* Write the signature last; native card serialization/checksums remain unchanged. */
    r[38] = 'T'; r[39] = 'Y';
    if (!signed_format || old_version < TM_SETTINGS_VERSION) return TM_SETTINGS_MIGRATED;
    return bytes_differ(before, r) ? TM_SETTINGS_REPAIRED : TM_SETTINGS_READY;
}

uint32_t TMSettings_Read(const uint8_t r[TM_SETTINGS_SIZE], unsigned field, unsigned index) {
    switch (field) {
    case TM_SETTING_OSD_MASK: return read_mask(r);
    case TM_SETTING_OSD_POSITION: return r[4];
    case TM_SETTING_EVENT_PAGE: return r[5];
    case TM_SETTING_RECOMMENDED: return r[6];
    case TM_SETTING_ADVANCE: return r[7] & 15;
    case TM_SETTING_DECREMENT: return r[7] >> 4;
    case TM_SETTING_DPAD_UP: return r[8] & 15;
    case TM_SETTING_DPAD_DOWN: return r[8] >> 4;
    case TM_SETTING_DPAD_LEFT: return r[9] & 15;
    case TM_SETTING_DPAD_RIGHT: return r[9] >> 4;
    case TM_SETTING_INPUT_DISPLAY: return r[11];
    case TM_SETTING_FLAG:
        if (index >= TM_FLAG_RUN_TURNAROUND && index < TM_FLAG_COUNT)
            return (r[TM_SETTINGS_EXTRA_FLAGS_OFFSET] >> (index - TM_FLAG_RUN_TURNAROUND)) & 1;
        return index < TM_FLAG_RUN_TURNAROUND ? (r[10] >> index) & 1 : 0;
    case TM_SETTING_OVERLAY_HMN: return index < TM_SETTINGS_OVERLAYS ? r[12 + index] & 15 : 0;
    case TM_SETTING_OVERLAY_CPU: return index < TM_SETTINGS_OVERLAYS ? r[12 + index] >> 4 : 0;
    case TM_SETTING_OSD_COLOR: {
        int slot = osd_slot(index);
        return slot >= 0 ? read_color(r, slot) : TM_COLOR_OFF;
    }
    case TM_SETTING_OSD_ENABLED: return index < 32 ? (read_mask(r) >> index) & 1 : 0;
    case TM_SETTING_NATIVE_ROW:
        if (TMSettings_NativeFlag(index) >= 0) return TMSettings_Read(r, TM_SETTING_FLAG, TMSettings_NativeFlag(index));
        return TMSettings_Read(r, TM_SETTING_OSD_ENABLED, index);
    default: return 0;
    }
}

int TMSettings_Write(uint8_t r[TM_SETTINGS_SIZE], unsigned field, unsigned index, uint32_t value) {
    if (r[38] != 'T' || r[39] != 'Y' || (r[10] >> 6) != TM_SETTINGS_VERSION) return 0;
    if (field == TM_SETTING_NATIVE_ROW) {
        if (TMSettings_NativeFlag(index) >= 0) return TMSettings_Write(r, TM_SETTING_FLAG, TMSettings_NativeFlag(index), value);
        return TMSettings_Write(r, TM_SETTING_OSD_ENABLED, index, value);
    }
    if (field == TM_SETTING_OSD_MASK) {
        uint8_t before[TM_SETTINGS_SIZE];
        memcpy(before, r, sizeof(before));
        write_mask(r, value);
        for (unsigned slot = 0; slot < TM_SETTINGS_OSDS; ++slot) {
            unsigned color = read_color(r, slot);
            if (!(value & (1u << TMSettings_OSDIDs[slot]))) color = TM_COLOR_OFF;
            else if (!color) color = TM_COLOR_WHITE;
            write_color(r, slot, color);
        }
        return bytes_differ(before, r);
    }
    if (field == TM_SETTING_OSD_ENABLED) {
        if (index >= 32 || value > 1) return 0;
        uint32_t mask = read_mask(r), bit = 1u << index;
        return TMSettings_Write(r, TM_SETTING_OSD_MASK, 0, value ? mask | bit : mask & ~bit);
    }
    unsigned byte, shift = 0, width = 8, count;
    switch (field) {
    case TM_SETTING_OSD_POSITION: byte = 4; count = 4; break;
    case TM_SETTING_EVENT_PAGE: byte = 5; count = 3; break;
    case TM_SETTING_RECOMMENDED: byte = 6; count = 2; break;
    case TM_SETTING_ADVANCE: byte = 7; width = 4; count = 5; break;
    case TM_SETTING_DECREMENT: byte = 7; width = 4; shift = 4; count = 6; break;
    case TM_SETTING_DPAD_UP: byte = 8; width = 4; count = 2; break;
    case TM_SETTING_DPAD_DOWN: byte = 8; width = 4; shift = 4; count = 3; break;
    case TM_SETTING_DPAD_LEFT: byte = 9; width = 4; count = 2; break;
    case TM_SETTING_DPAD_RIGHT: byte = 9; width = 4; shift = 4; count = 2; break;
    case TM_SETTING_INPUT_DISPLAY: byte = 11; count = 4; break;
    case TM_SETTING_FLAG:
        if (index >= TM_FLAG_COUNT) return 0;
        byte = index >= TM_FLAG_RUN_TURNAROUND ? TM_SETTINGS_EXTRA_FLAGS_OFFSET : 10;
        width = 1; shift = index >= TM_FLAG_RUN_TURNAROUND ? index - TM_FLAG_RUN_TURNAROUND : index; count = 2; break;
    case TM_SETTING_OVERLAY_HMN:
    case TM_SETTING_OVERLAY_CPU:
        if (index >= TM_SETTINGS_OVERLAYS) return 0;
        byte = 12 + index; width = 4; shift = field == TM_SETTING_OVERLAY_CPU ? 4 : 0;
        count = TM_SETTINGS_OVERLAY_CHOICES; break;
    case TM_SETTING_OSD_COLOR: {
        int slot = osd_slot(index);
        if (slot < 0 || value >= 8) return 0;
        unsigned old = read_color(r, slot);
        write_color(r, slot, value);
        uint32_t mask = read_mask(r), bit = 1u << index;
        uint32_t new_mask = value ? mask | bit : mask & ~bit;
        write_mask(r, new_mask);
        return old != value || mask != new_mask;
    }
    default: return 0;
    }
    if (value >= count) return 0;
    uint8_t old = r[byte];
    unsigned mask = ((1u << width) - 1) << shift;
    r[byte] = (r[byte] & ~mask) | (value << shift);
    return r[byte] != old;
}
