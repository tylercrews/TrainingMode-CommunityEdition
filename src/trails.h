#ifndef TM_TRAILS_H
#define TM_TRAILS_H
#include <stdint.h>

#define TM_TRAIL_CAPACITY 128
#define TM_TRAIL_CURRENT_ALPHA 216u
#define TM_TRAIL_HISTORY_ALPHA 84u
#define TM_TRAIL_FADE_BASE 200u /* Keep decay endpoints and Instant unchanged. */
enum TMTrailDecay {
    TM_TRAIL_NORMAL, TM_TRAIL_FAST, TM_TRAIL_VERY_FAST,
    TM_TRAIL_INSTANT, TM_TRAIL_SLOW, TM_TRAIL_NO_FADE, TM_TRAIL_DISABLED,
};
#define TM_TRAIL_DECAY_LABELS "Normal", "Fast", "Very Fast", "Instant", "Slow", "Off"

typedef struct TMTrailVec { float x, y, z; } TMTrailVec;
typedef struct TMTrailSample {
    TMTrailVec a, b;
    float size;
    uint32_t color, frame, source;
} TMTrailSample;
typedef struct TMTrailBank {
    TMTrailSample samples[TM_TRAIL_CAPACITY];
    uint32_t next, frame, has_frame;
} TMTrailBank;

void TMTrail_Clear(TMTrailBank *bank);
int TMTrail_BeginFrame(TMTrailBank *bank, uint32_t frame);
void TMTrail_Add(TMTrailBank *bank, const TMTrailSample *sample);
unsigned TMTrail_Alpha(unsigned mode, uint32_t age);
uint32_t TMTrail_DamageColor(uint32_t player_color, int damage);
unsigned TMTrail_SampleAlpha(unsigned mode, uint32_t age, uint32_t color);
unsigned TMTrail_Effective(unsigned very_fast, unsigned instant, unsigned local_enabled, unsigned local_mode);
uint32_t TMTrail_PlayerColor(unsigned accent, unsigned is_cpu);

typedef struct TMTrailAPI {
    void (*configure)(unsigned enabled, unsigned mode);
    void (*clear)(void);
} TMTrailAPI;
void Trails_MatchStart(void);
void Trails_SceneChange(void);
void Trails_Configure(unsigned enabled, unsigned mode);
void Trails_Clear(void);
#endif
