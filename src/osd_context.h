#ifndef TM_OSD_CONTEXT_H
#define TM_OSD_CONTEXT_H
#include <stdint.h>
/* Runtime side table: no fighter-layout, recording-format or save changes. */
typedef struct TMOSDContext {
    uint32_t frame, seen;
    int state, attack, airborne, shine, shield, victim;
    unsigned state_hl, previous_hl, air_hl, shine_hl, shield_hl, victim_hl, recovery_hl;
    int cancelled;
} TMOSDContext;
void TMOSDContext_Step(TMOSDContext *ctx, uint32_t frame, int state, int attack,
    int airborne, int shine, int shield, int victim, int hitlag, int dead);
unsigned TMOSDContext_Hitlag(const TMOSDContext *ctx, unsigned category);
/* Return the native opportunity count and anchor/source indexes; 0 = no result. */
int TMOSD_WaitFrames(const uint16_t *states, const uint16_t *frames, unsigned count,
    unsigned normal_lag, int *anchor, int *source);
#endif
