#include "events.h"
#include "osds.h"
#include "osd_context.h"
typedef struct OSDContextFighter { FighterData *fighter; int spawn; TMOSDContext context; TMShineEpisode shine; } OSDContextFighter;
typedef char osd_context_layout[(sizeof(TMOSDContext) == 64 && sizeof(TMShineEpisode) == 52 && sizeof(OSDContextFighter) == 124) ? 1 : -1];
static OSDContextFighter contexts[12];
static OSDContextFighter *entry_for(FighterData *fighter) {
    unsigned slot = (unsigned)(u8)fighter->ply * 2 + !!fighter->flags.ms;
    if (slot >= countof(contexts)) return 0;
    OSDContextFighter *entry = &contexts[slot];
    if (entry->fighter != fighter || entry->spawn != fighter->spawn_num)
        *entry = (OSDContextFighter){.fighter = fighter, .spawn = fighter->spawn_num};
    return entry;
}
static TMOSDContext *context_for(FighterData *fighter) {
    OSDContextFighter *entry = entry_for(fighter);
    return entry ? &entry->context : 0;
}
void OSDContext_Clear(void) { memset(contexts, 0, sizeof(contexts)); }
/* 0x8006AB78 is reached by both frozen and normal animation updates, before
 * native IASA/physics OSD producers. Never rely on a late event callback. */
void OSDContext_Tick(FighterData *fighter) {
    TMOSDContext *c = context_for(fighter);
    if (!c) return;
    int state = fighter->state_id;
    int shine = (fighter->kind == FTKIND_FOX || fighter->kind == FTKIND_FALCO) && state >= 360 && state <= 369;
    OSDContextFighter *entry = entry_for(fighter);
    TMShine_Tick(&entry->shine, stc_match->time_frames, shine ? state : -1,
        fighter->flags.hitlag || (fighter->flags.freeze && fighter->dmg.hitlag_frames > 0), fighter->flags.dead);
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
    Message_Display(OSD_MessageTag(5, OSD_ActOoWait, 2, 2, 0) | TM_OSD_POINTER_FIRST, data->ply, MSGCOLOR_WHITE,
        "Act OoWait\n%s\n%df", label, frame);
}

void OSD_ShineBeforeIASA(GOBJ *object) {
    if (!object || !object->userdata) return;
    FighterData *data = object->userdata;
    if (data->kind != FTKIND_FOX && data->kind != FTKIND_FALCO) return;
    OSDContextFighter *entry = entry_for(data);
    if (entry) TMShine_Before(&entry->shine, stc_match->time_frames, data->state_id,
        data->flags.freeze || data->flags.dead,
        data->state_id == 361 || (u8)data->jump.jumps_used < data->attr.max_jumps);
}
void OSD_ShineAfterIASA(GOBJ *object) {
    if (!object || !object->userdata) return;
    FighterData *data = object->userdata;
    if (data->kind != FTKIND_FOX && data->kind != FTKIND_FALCO) return;
    OSDContextFighter *entry = entry_for(data);
    if (!entry) return;
    TMShineEpisode *shine = &entry->shine;
    int result = TMShine_After(shine, stc_match->time_frames, data->state_id,
        data->flags.freeze || data->flags.dead);
    if (!result || data->flags.ms || !(Settings_Get(TM_SETTING_OSD_MASK, 0) & (1u << OSD_FighterSpecificTech))) return;
    unsigned frame = result == TM_SHINE_JUMP ? shine->jump_opportunity : shine->opportunity;
    GOBJ *message = Message_Display(OSD_MessageTag(OSD_FighterSpecificTech, OSD_FighterSpecificTech, 1, 1, 0) | TM_OSD_DEFER_FORMAT,
        data->ply, MSGCOLOR_WHITE, "Jump Out Of Shine\n%df", frame);
    MsgData *msg = message->userdata;
    msg->timing_hitlag = shine->hitlag;
    msg->timing_turn = shine->first_turn;
    msg->timing_second_turn = result == TM_SHINE_DOUBLE_TURN;
    OSD_FormatTiming(msg, 0, -MSGTEXT_YOFFSET / 2);
    OSD_ApplyMessageStyle(msg);
}
