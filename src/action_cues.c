#include "action_cues.h"
#include <float.h>

/* Native skip sizes at 0x803C0870, verified against the input DOL. */
static const uint8_t command_words[] = {
    5,5,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,
    1,1,1,7,4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,3,2,1,4,
};
static int readable(const uint32_t *p, unsigned words) {
    uintptr_t a = (uintptr_t)p;
    return !(a & 3) && a >= 0x80000000u && a <= 0x81800000u - words * 4;
}
int TMCue_Remaining(float frame, float rate, float boundary) {
    if (!(rate > 0) || !(boundary >= 0) || !(frame >= -1)) return -1;
    if (frame >= boundary) return 0;
    if (frame + rate >= boundary) return 1;
    if (frame + 2 * rate >= boundary) return 2;
    return 3;
}
int TMCue_ScriptIASA(const TMCueScript *source, float rate) {
    if (!(rate > 0) || source->depth > 5) return -1;
    TMCueScript copy = *source;
    /* Read only: no hitboxes, sound, randomness or live interpreter mutations. */
    for (unsigned step = 1; step <= 2; ++step) {
        copy.frame += rate;
        if (copy.timer != FLT_MAX) copy.timer -= rate;
        for (unsigned budget = 0; budget < 256; ++budget) {
            if (!copy.current) return 3;
            if (copy.timer == FLT_MAX) {
                if (copy.frame >= rate) return 3;
                copy.timer = -copy.frame;
            }
            if (copy.timer > 0) break;
            if (!readable(copy.current, 1)) return -1;
            unsigned word = *copy.current, op = word >> 26, value = word & 0x03FFFFFFu;
            if (op == 22) return step; /* 0x58: Enable IASA. */
            switch (op) {
            case 0: return 3;
            case 1: copy.timer += value; copy.current++; break;
            case 2: copy.timer = value - copy.frame; copy.current++; break;
            case 3:
                if (copy.depth > 3) return -1;
                copy.stack[copy.depth++] = (uintptr_t)(copy.current + 1);
                copy.stack[copy.depth++] = value;
                copy.current++;
                break;
            case 4:
                if (copy.depth < 2) return -1;
                if (--copy.stack[copy.depth - 1])
                    copy.current = (const uint32_t *)copy.stack[copy.depth - 2];
                else { copy.depth -= 2; copy.current++; }
                break;
            case 5:
                if (copy.depth >= 5 || !readable(copy.current, 2)) return -1;
                copy.stack[copy.depth++] = (uintptr_t)(copy.current + 2);
                copy.current = (const uint32_t *)(uintptr_t)copy.current[1];
                break;
            case 6:
                if (!copy.depth) return -1;
                copy.current = (const uint32_t *)copy.stack[--copy.depth];
                break;
            case 7:
                if (!readable(copy.current, 2)) return -1;
                copy.current = (const uint32_t *)(uintptr_t)copy.current[1];
                break;
            case 8: copy.current++; copy.timer = FLT_MAX; break;
            case 9: copy.current++; break; /* Background flash: deliberately skip. */
            default:
                if (op < 10 || op >= 10 + sizeof(command_words)) return -1;
                unsigned words = command_words[op - 10];
                if (!readable(copy.current, words)) return -1;
                copy.current += words;
            }
            if (budget == 255) return -1; /* Cyclic/invalid script: no guessed boundary. */
        }
    }
    return 3;
}
unsigned TMCue_Update(TMCueState *cue, int state, float frame, unsigned instance,
                      int remaining, int frozen, int timing, int red_entry) {
    if (frozen) return cue->red ? TM_CUE_RED : TM_CUE_NONE;
    if (cue->seen && cue->state == state && cue->instance == instance && frame < cue->frame)
        *cue = (TMCueState){0};
    if (cue->green) cue->green--;
    if (cue->red) cue->red--;
    int changed = !cue->seen || state != cue->state || instance != cue->instance;
    int recovered = cue->seen && cue->remaining > 0 &&
        (remaining == 0 || (changed && cue->remaining == 1 && remaining >= 0));
    if (timing && recovered) cue->green = 2;
    if (!timing || remaining < 0) cue->green = 0;
    if (changed && red_entry) { cue->red = 4; cue->red_source = red_entry; }
    cue->seen = 1; cue->state = state; cue->frame = frame;
    cue->instance = instance; cue->remaining = remaining;
    if (timing && (remaining == 1 || remaining == 2)) return TM_CUE_YELLOW;
    if (timing && cue->green) return TM_CUE_GREEN;
    return cue->red ? TM_CUE_RED : TM_CUE_NONE;
}
