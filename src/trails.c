#include "trails.h"
#include <string.h>

void TMTrail_Clear(TMTrailBank *bank) { memset(bank, 0, sizeof(*bank)); }

int TMTrail_BeginFrame(TMTrailBank *bank, uint32_t frame) {
    if (bank->has_frame && frame == bank->frame) return 0;
    if (bank->has_frame && (frame < bank->frame || frame - bank->frame > 1))
        TMTrail_Clear(bank); /* Restore, rewind or discontinuous recording timeline. */
    bank->frame = frame;
    bank->has_frame = 1;
    return 1;
}

static int same_vec(TMTrailVec a, TMTrailVec b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

void TMTrail_Add(TMTrailBank *bank, const TMTrailSample *sample) {
    if (sample->size <= 0) return;
    /* A frozen hitbox should remain current, without stacking identical translucent copies. */
    for (unsigned i = 0; i < TM_TRAIL_CAPACITY; ++i) {
        TMTrailSample *old = &bank->samples[i];
        if (old->size > 0 && old->source == sample->source &&
            sample->frame >= old->frame && sample->frame - old->frame <= 1 &&
            old->size == sample->size && old->color == sample->color &&
            same_vec(old->a, sample->a) && same_vec(old->b, sample->b)) {
            old->frame = sample->frame;
            return;
        }
    }
    bank->samples[bank->next] = *sample;
    bank->next = (bank->next + 1) % TM_TRAIL_CAPACITY;
}

unsigned TMTrail_Alpha(unsigned mode, uint32_t age) {
    static const uint8_t hold[] = {15, 10, 5, 0, 30, 0};
    static const uint8_t fade[] = {4, 8, 13, 200, 2, 0};
    if (mode >= TM_TRAIL_DISABLED) return 0;
    if (age == 0) return TM_TRAIL_CURRENT_ALPHA;
    unsigned lost = 0;
    if (fade[mode] && age > hold[mode]) {
        uint32_t elapsed = age - hold[mode];
        if (elapsed >= (TM_TRAIL_CURRENT_ALPHA + fade[mode] - 1) / fade[mode]) return 0;
        lost = elapsed * fade[mode];
    }
    return (TM_TRAIL_HISTORY_ALPHA * (TM_TRAIL_CURRENT_ALPHA - lost) + TM_TRAIL_CURRENT_ALPHA / 2) / TM_TRAIL_CURRENT_ALPHA;
}
uint32_t TMTrail_DamageColor(uint32_t player_color, int damage) {
    /* Base damage avoids stale-move/handicap changes to the visual cue. Strong
     * and weak phases/simultaneous hitboxes remain distinct in saved samples. */
    /* A steeper curve makes strong/late hit phases visibly different instead
     * of raising both toward the same opacity ceiling. Clamp before squaring. */
    unsigned alpha = damage <= 0 ? 96 : damage >= 12 ? 255 :
        96 + ((unsigned)damage * (unsigned)damage * 159 + 72) / 144;
    return (player_color & 0xFFFFFF00u) | alpha;
}
unsigned TMTrail_SampleAlpha(unsigned mode, uint32_t age, uint32_t color) {
    unsigned fade = TMTrail_Alpha(mode, age);
    if (!fade) return 0;
    unsigned strength = color & 255;
    if (!age) return strength;
    if (!strength) return 0;
    unsigned history = strength < 96 ? (strength * 48 + 48) / 96 :
        48 + ((strength - 96) * 80 + 79) / 159;
    unsigned alpha = (fade * history + TM_TRAIL_HISTORY_ALPHA / 2) / TM_TRAIL_HISTORY_ALPHA;
    return alpha ? alpha : (strength ? 1 : 0);
}

unsigned TMTrail_Effective(unsigned very_fast, unsigned instant, unsigned local_enabled, unsigned local_mode) {
    /* Very Fast includes Instant's age-zero geometry; render that union only once. */
    if (very_fast) return TM_TRAIL_VERY_FAST;
    if (instant) return TM_TRAIL_INSTANT;
    return local_enabled && local_mode < TM_TRAIL_DISABLED ? local_mode : TM_TRAIL_DISABLED;
}

uint32_t TMTrail_PlayerColor(unsigned accent, unsigned is_cpu) {
    static const uint32_t colors[] = {0xFF4646C8, 0x4691FFC8, 0xFFE141C8, 0x4BE164C8};
    return !is_cpu && accent < 4 ? colors[accent] : 0xB4B4B4C8;
}
