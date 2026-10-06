#include "osd_context.h"
#include "../MexTK/mex.h"
static void increment(unsigned *value) { if (*value < 999) ++*value; }
void TMOSDContext_Step(TMOSDContext *c, uint32_t frame, int state, int attack,
    int airborne, int shine, int shield, int victim, int hitlag, int dead) {
    if (c->seen && frame == c->frame) return;
    if (dead || state <= ASID_REBIRTHWAIT ||
        (state >= ASID_CLIFFCATCH && state <= ASID_CLIFFWAIT) || (c->seen && frame < c->frame)) {
        *c = (TMOSDContext){0};
        c->seen = 1; c->frame = frame; c->state = state; c->attack = attack;
        return;
    }
    int changed = c->seen && (state != c->state || attack != c->attack);
    if (changed) {
        c->previous_hl = c->state_hl;
        int was_landing = c->state == ASID_LANDING || c->state == ASID_LANDINGFALLSPECIAL ||
            (c->state >= ASID_LANDINGAIRN && c->state <= ASID_LANDINGAIRLW);
        if ((state == ASID_WAIT && !was_landing) || state == ASID_LANDING || state == ASID_LANDINGFALLSPECIAL ||
            (state >= ASID_LANDINGAIRN && state <= ASID_LANDINGAIRLW))
            c->recovery_hl = c->airborne ? c->air_hl : c->state_hl;
        c->state_hl = 0;
    }
    if (airborne && !c->airborne) c->air_hl = 0;
    if (shine && !c->shine) c->shine_hl = 0;
    if (shield && !c->shield) c->shield_hl = 0;
    if (victim && !c->victim) c->victim_hl = 0;
    if (hitlag) {
        increment(&c->state_hl);
        int landing = state == ASID_LANDING || state == ASID_LANDINGFALLSPECIAL ||
            (state >= ASID_LANDINGAIRN && state <= ASID_LANDINGAIRLW);
        if (airborne || landing) increment(&c->air_hl);
        if (landing || state == ASID_WAIT) increment(&c->recovery_hl);
        if (shine) increment(&c->shine_hl);
        if (shield) increment(&c->shield_hl);
        if (victim) increment(&c->victim_hl);
    }
    c->seen = 1; c->frame = frame; c->state = state; c->attack = attack;
    c->airborne = airborne; c->shine = shine; c->shield = shield; c->victim = victim;
}
unsigned TMOSDContext_Hitlag(const TMOSDContext *c, unsigned category) {
    switch (category) {
    case 0: case 1: case 18: case 20: return c->air_hl;
    case 3: return c->shield_hl;
    case 8: return c->shine ? c->shine_hl : c->state_hl;
    case 16: return c->recovery_hl;
    case 28: return c->victim_hl;
    default: return c->state_hl;
    }
}
int TMOSD_WaitFrames(const uint16_t *states, const uint16_t *frames, unsigned count,
    unsigned normal_lag, int *anchor, int *source) {
    if (count > 6) count = 6;
    unsigned i = 0;
    for (; i < count; ++i) {
        int state = states[i];
        if (state == ASID_WAIT || state == ASID_LANDING || state == ASID_LANDINGFALLSPECIAL ||
            (state >= ASID_LANDINGAIRN && state <= ASID_LANDINGAIRLW)) break;
        if (state != ASID_WALKSLOW && state != ASID_WALKMIDDLE && state != ASID_WALKFAST &&
            state != ASID_TURN && state != ASID_SQUAT) return 0;
    }
    if (i == count) return 0;
    *anchor = i;
    *source = states[i] == ASID_WAIT ? i + 1 : i;
    if ((unsigned)*source >= count) return 0;
    int result = 1 - (states[i] == ASID_LANDING ? (int)normal_lag : 0);
    if (states[i] == ASID_LANDINGFALLSPECIAL ||
        (states[i] >= ASID_LANDINGAIRN && states[i] <= ASID_LANDINGAIRLW)) result -= frames[i];
    for (unsigned j = 0; j <= i; ++j) {
        result += frames[j];
        if (states[j] == ASID_TURN || states[j] == ASID_WAIT) --result;
    }
    return result >= 1 && result <= 13 ? result : 0;
}
