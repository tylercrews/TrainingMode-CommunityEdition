#include "events.h"
#include "osds.h"
#include "osd_context.h"
typedef struct OSDContextFighter { FighterData *fighter; int spawn; TMOSDContext context; } OSDContextFighter;
typedef char osd_context_layout[(sizeof(TMOSDContext) == 64 && sizeof(OSDContextFighter) == 72) ? 1 : -1];
static OSDContextFighter contexts[12];
static TMOSDContext *context_for(FighterData *fighter) {
    unsigned slot = (unsigned)(u8)fighter->ply * 2 + !!fighter->flags.ms;
    if (slot >= countof(contexts)) return 0;
    OSDContextFighter *entry = &contexts[slot];
    if (entry->fighter != fighter || entry->spawn != fighter->spawn_num)
        *entry = (OSDContextFighter){.fighter = fighter, .spawn = fighter->spawn_num};
    return &entry->context;
}
void OSDContext_Clear(void) { memset(contexts, 0, sizeof(contexts)); }
/* 0x8006AB78 is reached by both frozen and normal animation updates, before
 * native IASA/physics OSD producers. Never rely on a late event callback. */
void OSDContext_Tick(FighterData *fighter) {
    TMOSDContext *c = context_for(fighter);
    if (!c) return;
    int state = fighter->state_id;
    int shine = (fighter->kind == FTKIND_FOX || fighter->kind == FTKIND_FALCO) && state >= 360 && state <= 369;
    int shield = state >= ASID_GUARDON && state <= ASID_GUARDREFLECT;
    int victim = (state >= ASID_DAMAGEHI1 && state <= ASID_DAMAGEFLYROLL) || state == ASID_DAMAGEFALL;
    TMOSDContext_Step(c, stc_match->time_frames, state, fighter->atk_instance,
        fighter->phys.air_state, shine, shield, victim,
        fighter->flags.hitlag || (fighter->flags.freeze && fighter->dmg.hitlag_frames > 0), fighter->flags.dead);
}
void OSDContext_LCancel(GOBJ *fighter) {
    FighterData *data = fighter->userdata;
    TMOSDContext *c = context_for(data);
    if (c) c->cancelled = (u8)data->input.timer_trigger_any_ignore_hitlag < (**stc_ftcommon).lcancel_input_window;
}
unsigned OSDContext_MessageHitlag(int queue, unsigned category) {
    if ((unsigned)queue >= 6) return 0;
    GOBJ *object = Fighter_GetSubcharGObj(queue, 0);
    if (!object || !object->userdata) return 0;
    TMOSDContext *c = context_for(object->userdata);
    return c ? TMOSDContext_Hitlag(c, category) : 0;
}
static const char *wait_source(FighterData *data, int index) {
    int state = ActionCues_CommonState(data->kind, data->TM.state_prev[index]);
    int before = index < 5 ? ActionCues_CommonState(data->kind, data->TM.state_prev[index + 1]) : -1;
    if (state == ASID_LANDING) return before >= ASID_ATTACKAIRN && before <= ASID_ATTACKAIRLW ? "Autocancel" : "Landing";
    if (state >= ASID_LANDINGAIRN && state <= ASID_LANDINGAIRLW) {
        if (data->kind == FTKIND_GAW) return "Aerial landing";
        TMOSDContext *c = context_for(data);
        return c && c->cancelled ? "L-cancel" : "Missed L-cancel";
    }
    if (state == ASID_LANDINGFALLSPECIAL) return before == ASID_ESCAPEAIR ? "Waveland" : "Special landing";
    if (state >= ASID_THROWF && state <= ASID_THROWLW) return "Throw";
    if (state == ASID_PASSIVE) return "Tech";
    if (state == ASID_PASSIVESTANDF || state == ASID_PASSIVESTANDB) return "Tech roll";
    if (state == ASID_DOWNATTACKU || state == ASID_DOWNATTACKD) return "Getup attack";
    if (state >= ASID_DOWNBOUNDU && state <= ASID_PASSIVESTANDB) return "Getup";
    if ((data->kind == FTKIND_FOX || data->kind == FTKIND_FALCO) && state >= 341 && state <= 346) return "Laser";
    if (data->kind == FTKIND_SHEIK && state >= 341 && state <= 348) return "Needles";
    if (data->kind == FTKIND_PEACH && state >= 344 && state <= 348) return "Float aerial";
    if (state >= ASID_ATTACKAIRN && state <= ASID_ATTACKAIRLW) return "Aerial";
    return 0;
}
void OSD_ActOutWait(GOBJ *object) {
    FighterData *data = object->userdata;
    if (data->flags.ms || data->TM.state_frame > 1) return;
    uint16_t states[6];
    for (unsigned i = 0; i < 6; ++i) states[i] = ActionCues_CommonState(data->kind, data->TM.state_prev[i]);
    int anchor, source;
    float lag = data->attr.normal_landing_lag;
    unsigned unavoidable = lag > 0 ? (unsigned)lag + (lag > (unsigned)lag) : 0;
    int frame = TMOSD_WaitFrames(states, data->TM.state_prev_frames, 6, unavoidable, &anchor, &source);
    if (!frame) return;
    const char *label = wait_source(data, source);
    if (!label) return;
    Message_Display(OSD_MessageTag(5, OSD_ActOoWait, 1, 1, 0), data->ply, MSGCOLOR_WHITE,
        "Act OoWait\nFrame %d\n%s", frame, label);
}
