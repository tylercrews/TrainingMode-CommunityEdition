#ifndef TM_ACTION_CUES_H
#define TM_ACTION_CUES_H
#include <stdint.h>

enum { TM_CUE_NONE, TM_CUE_YELLOW, TM_CUE_GREEN, TM_CUE_RED };
/* Native command interpreter snapshot at Fighter + 0x3E4, including its stack. */
typedef struct TMCueScript {
    float timer, frame;
    const uint32_t *current;
    unsigned depth;
    uintptr_t stack[5];
} TMCueScript;
typedef struct TMCueState {
    int seen, state, remaining, green, red, red_source;
    float frame;
    unsigned instance;
} TMCueState;
/* -1 unknown; 0 actionable; 1/2 within the yellow window; 3 further away. */
int TMCue_Remaining(float frame, float rate, float boundary);
int TMCue_ScriptIASA(const TMCueScript *source, float rate);
unsigned TMCue_Update(TMCueState *cue, int state, float frame, unsigned instance,
                      int remaining, int frozen, int timing, int red_entry);
void ActionCues_Clear(void);
void ActionCues_SceneChange(void);
void ActionCues_MatchStart(void);
#endif
