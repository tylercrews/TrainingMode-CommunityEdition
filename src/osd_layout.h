#ifndef TM_OSD_LAYOUT_H
#define TM_OSD_LAYOUT_H
#include <stdint.h>
#include "settings.h"

#define TM_OSD_PLAYERS 6
#define TM_OSD_KEYS (TM_OSD_PLAYERS * TM_SETTINGS_OSDS)
#define TM_OSD_GRID_COLUMNS 5
#define TM_OSD_GRID_CELLS 10
#define TM_OSD_PANEL_CELLS 9
#define TM_OSD_HISTORY 4 /* Latest + three previous scores. */

typedef struct TMOSDHistory {
    int frame[TM_OSD_HISTORY];
    uint32_t color[TM_OSD_HISTORY];
    uint32_t native_frame;
    int kind;
    uint8_t count, turn[TM_OSD_HISTORY], seen;
} TMOSDHistory;
typedef struct TMOSDMap {
    uint32_t mask;
    uint8_t owners[TM_OSD_PLAYERS], owner_count;
} TMOSDMap;
typedef struct TMOSDCell {
    int key, page, cell;
    float x, y;
} TMOSDCell;

int TMOSD_Key(int player, int category);
int TMOSD_Category(int key);
void TMOSD_MapReset(TMOSDMap *map, uint32_t mask);
void TMOSD_MapOwners(TMOSDMap *map, unsigned eligible);
unsigned TMOSD_Count(const TMOSDMap *map);
unsigned TMOSD_Capacity(unsigned layout);
unsigned TMOSD_PageCount(const TMOSDMap *map, unsigned layout);
TMOSDCell TMOSD_Cell(const TMOSDMap *map, int key, unsigned layout);
void TMOSD_HistoryPush(TMOSDHistory *history, int kind, int frame, uint32_t color,
                       int turn, uint32_t native_frame);
#endif
