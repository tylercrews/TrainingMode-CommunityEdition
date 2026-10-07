#include "osd_layout.h"
#include <string.h>

int TMOSD_Key(int player, int category) {
    if ((unsigned)player >= TM_OSD_PLAYERS) return -1;
    for (unsigned i = 0; i < TM_SETTINGS_OSDS; ++i)
        if (TMSettings_OSDIDs[i] == category) return player * TM_SETTINGS_OSDS + i;
    return -1;
}
int TMOSD_Category(int key) { return (unsigned)key < TM_OSD_KEYS ? TMSettings_OSDIDs[key % TM_SETTINGS_OSDS] : -1; }
void TMOSD_MapReset(TMOSDMap *map, uint32_t mask) { memset(map, 0, sizeof(*map)); map->mask = mask; }
void TMOSD_MapOwners(TMOSDMap *map, unsigned eligible) {
    for (unsigned player = 0; player < TM_OSD_PLAYERS; ++player) {
        if (!(eligible & (1u << player))) continue;
        unsigned i = 0;
        while (i < map->owner_count && map->owners[i] != player) ++i;
        if (i == map->owner_count && i < TM_OSD_PLAYERS) map->owners[map->owner_count++] = player;
    }
}
static unsigned categories(const TMOSDMap *map) {
    unsigned count = 0;
    for (unsigned i = 0; i < TM_SETTINGS_OSDS; ++i) count += !!(map->mask & (1u << TMSettings_OSDIDs[i]));
    return count;
}
unsigned TMOSD_Count(const TMOSDMap *map) { return categories(map) * map->owner_count; }
unsigned TMOSD_Capacity(unsigned layout) { return layout == TM_OSD_PANEL ? TM_OSD_PANEL_CELLS : TM_OSD_GRID_CELLS; }
unsigned TMOSD_PageCount(const TMOSDMap *map, unsigned layout) {
    unsigned count = TMOSD_Count(map), capacity = TMOSD_Capacity(layout);
    return count ? (count + capacity - 1) / capacity : 1;
}
TMOSDCell TMOSD_Cell(const TMOSDMap *map, int key, unsigned layout) {
    TMOSDCell out = {.key = -1, .page = -1, .cell = -1};
    int category = TMOSD_Category(key);
    if (category < 0 || !(map->mask & (1u << category))) return out;
    unsigned owner = 0;
    while (owner < map->owner_count && map->owners[owner] != key / TM_SETTINGS_OSDS) ++owner;
    if (owner == map->owner_count) return out;
    unsigned index = owner * categories(map);
    for (unsigned i = 0; i < (unsigned)key % TM_SETTINGS_OSDS; ++i)
        index += !!(map->mask & (1u << TMSettings_OSDIDs[i]));
    unsigned capacity = TMOSD_Capacity(layout);
    out.key = key; out.page = index / capacity; out.cell = index % capacity;
    out.x = layout == TM_OSD_PANEL ? -26.f : -19.f + (out.cell % 3) * 19.f;
    out.y = layout == TM_OSD_PANEL ? 18.f - out.cell * 5.3f : 18.f - (out.cell / 3) * 7.f;
    return out;
}
void TMOSD_HistoryPush(TMOSDHistory *h, int kind, int frame, uint32_t color, int turn, uint32_t native_frame) {
    if (frame < 0) return;
    if (h->seen && h->kind == kind && h->native_frame == native_frame && h->count &&
        h->frame[h->count - 1] == frame && h->turn[h->count - 1] == !!turn) return;
    if (h->seen && h->kind != kind) h->count = 0; /* Do not mix unrelated category sub-techniques. */
    if (h->count == TM_OSD_HISTORY) {
        for (unsigned i = 0; i < TM_OSD_HISTORY - 1; ++i) {
            h->frame[i] = h->frame[i + 1]; h->color[i] = h->color[i + 1]; h->turn[i] = h->turn[i + 1];
        }
        --h->count;
    }
    unsigned i = h->count++;
    h->frame[i] = frame; h->color[i] = color; h->turn[i] = !!turn;
    h->kind = kind; h->native_frame = native_frame; h->seen = 1;
}
