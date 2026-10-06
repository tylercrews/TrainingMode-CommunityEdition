#include "events.h"
#include "action_cues.h"
#include <stddef.h>

typedef char cue_script_layout[(sizeof(TMCueScript) == 0x24 &&
    offsetof(FighterData, script) == 0x3E4 && offsetof(FighterData, color) == 0x408) ? 1 : -1];
typedef struct CueFighter {
    GOBJ *object;
    int spawn;
    void (*draw)(GOBJ *, int);
    TMCueState cue;
    unsigned color;
    int missed_entry;
    GXColor local_color;
    int local_active;
} CueFighter;
static CueFighter fighters[12];
static GOBJ *manager;
static unsigned last_frame;
static int seen_frame, live;
static void Cue_Draw(GOBJ *gobj, int pass);
int ActionCues_IsProtected(GOBJ *gobj) {
    if (!gobj || !gobj->userdata) return 0;
    FighterData *data = gobj->userdata;
    if (data->flags.dead) return 0;
    if (data->hurt.kind_script > 0 || data->hurt.kind_game > 0 ||
        Fighter_GetIntangibleFrames(gobj) > 0 || (data->kind == FTKIND_YOSHI && data->dmg.armor > 0)) return 1;
    /* SetAllHurtCapsules (script opcode 0x6C) does not set the aggregate status.
     * Include whole-body protection through that path, excluding a lone limb. */
    if (!data->hurt_num || data->hurt_num > countof(data->hurtbox)) return 0;
    for (unsigned i = 0; i < data->hurt_num; ++i)
        if (data->hurtbox[i].state <= 0) return 0;
    return 1;
}

static CueFighter *get_fighter(GOBJ *gobj) {
    FighterData *data = gobj->userdata;
    unsigned slot = (unsigned)(u8)data->ply * 2 + !!data->flags.ms;
    if (slot >= countof(fighters)) return 0;
    CueFighter *entry = &fighters[slot];
    if (entry->object != gobj) {
        *entry = (CueFighter){.object = gobj, .spawn = data->spawn_num};
        entry->missed_entry = -1;
    } else if (entry->spawn != data->spawn_num) {
        /* Native respawn reuses the GOBJ. Keep its original renderer even when
         * gx_cb is already our wrapper; losing it makes the reborn fighter vanish. */
        entry->spawn = data->spawn_num;
        entry->cue = (TMCueState){0}; entry->color = TM_CUE_NONE; entry->missed_entry = -1;
        entry->local_active = 0;
    }
    int wrap = entry->local_active || Settings_Get(TM_SETTING_FLAG, TM_FLAG_LAST_BLOCKED_FRAME) ||
        Settings_Get(TM_SETTING_FLAG, TM_FLAG_MISSED_LCANCEL) ||
        Settings_Get(TM_SETTING_FLAG, TM_FLAG_RUN_TURNAROUND) ||
        Settings_Get(TM_SETTING_FLAG, TM_FLAG_INVINCIBILITY);
    if (gobj->gx_cb && gobj->gx_cb != Cue_Draw) {
        entry->draw = gobj->gx_cb;
        if (wrap) gobj->gx_cb = Cue_Draw;
    } else if (!wrap && entry->draw) gobj->gx_cb = entry->draw;
    return entry;
}
void ActionCues_SetLocalOverlay(GOBJ *gobj, const GXColor *color) {
    CueFighter *entry = get_fighter(gobj);
    if (!entry) return;
    entry->local_active = color != 0;
    if (color) entry->local_color = *color;
    get_fighter(gobj); /* Reconcile the draw wrapper after changing local ownership. */
}

/* Normalize exceptional ordinary attacks; these are not character specials. */
int ActionCues_CommonState(int kind, int state) {
    if (kind == FTKIND_GAW && state >= 341 && state <= 352) {
        static const short common[] = {
            ASID_ATTACK11, ASID_ATTACK100START, ASID_ATTACK100LOOP, ASID_ATTACK100END,
            ASID_ATTACKLW3, ASID_ATTACKS4S, ASID_ATTACKAIRN, ASID_ATTACKAIRB, ASID_ATTACKAIRHI,
            ASID_LANDINGAIRN, ASID_LANDINGAIRB, ASID_LANDINGAIRHI,
        };
        return common[state - 341];
    }
    /* Kirby's grounded dash attack and its aerial continuation share recovery. */
    if (kind == FTKIND_KIRBY && (state == 351 || state == 352)) return ASID_ATTACKDASH;
    return state;
}
static int neutral(int state) {
    return (state >= ASID_WAIT && state <= ASID_RUN) ||
        (state >= ASID_JUMPF && state <= ASID_FALLAERIALB) ||
        state == ASID_SQUATWAIT || state == ASID_OTTOTTOWAIT ||
        state == ASID_GUARD || state == ASID_GUARDREFLECT || state == ASID_PASS;
}
int ActionCues_Remaining(GOBJ *gobj) {
    FighterData *data = gobj->userdata;
    int state = ActionCues_CommonState(data->kind, data->state_id);
    if (neutral(state)) return 0;
    int landing = state == ASID_LANDING || state == ASID_LANDINGFALLSPECIAL ||
        (state >= ASID_LANDINGAIRN && state <= ASID_LANDINGAIRLW);
    int attack = state >= ASID_ATTACK11 && state <= ASID_ATTACKAIRLW;
    if ((!landing && !attack) || state == ASID_ATTACK100START ||
        state == ASID_ATTACK100LOOP || data->action_flags.loop_anim || !(data->state.rate > 0))
        return -1;
    float end = Fighter_GetCurrentAnimLength(gobj);
    if (!(end > 0)) return -1;
    float boundary = end;
    if ((state == ASID_LANDING || state == ASID_LANDINGFALLSPECIAL) &&
        data->state_var.state_var1 && data->attr.normal_landing_lag < boundary)
        boundary = data->attr.normal_landing_lag;
    int remaining = TMCue_Remaining(data->state.frame, data->state.rate, boundary);
    if (attack && !(data->kind == FTKIND_KIRBY && data->state_id == 352)) {
        if (data->flags.past_iasa) return 0; /* These attack entries clear the bit themselves. */
        TMCueScript script;
        memcpy(&script, &data->script, sizeof(script));
        int iasa = TMCue_ScriptIASA(&script, data->state.rate);
        if (iasa >= 0 && iasa < remaining) remaining = iasa;
    }
    return remaining;
}

/* Called at the actual aerial landing entry, before the trigger timer can age.
 * Autocancels bypass this entry; G&W's uncancellable custom landing states are
 * not counted as missed cancels. Native rate scaling remains authoritative. */
void ActionCues_LCancel(GOBJ *gobj) {
    if (!live || !gobj || !gobj->userdata) return;
    FighterData *data = gobj->userdata;
    if (data->state_id < ASID_LANDINGAIRN || data->state_id > ASID_LANDINGAIRLW) return;
    CueFighter *entry = get_fighter(gobj);
    if (entry) entry->missed_entry = (u8)data->input.timer_trigger_any_ignore_hitlag >= (**stc_ftcommon).lcancel_input_window;
}
static void full_shields(void) {
    if (!live || !Settings_Get(TM_SETTING_FLAG, TM_FLAG_INFINITE_SHIELDS)) return;
    float health = (**stc_ftcommon).x260;
    for (int ply = 0; ply < 6; ++ply)
        for (int sub = 0; sub < 2; ++sub) {
            GOBJ *ft = Fighter_GetSubcharGObj(ply, sub);
            if (ft && !((FighterData *)ft->userdata)->flags.dead)
                ((FighterData *)ft->userdata)->shield.health = health;
        }
}
static void Cue_Early(GOBJ *gobj) { full_shields(); }
static void Cue_Think(GOBJ *gobj) {
    if (!live) return;
    full_shields(); /* Runs after event-local shield writes. */
    unsigned frame = stc_match->time_frames;
    if (seen_frame && frame == last_frame) return;
    if (seen_frame && frame < last_frame) ActionCues_Clear();
    seen_frame = 1; last_frame = frame;
    int timing = Settings_Get(TM_SETTING_FLAG, TM_FLAG_LAST_BLOCKED_FRAME);
    int missed = Settings_Get(TM_SETTING_FLAG, TM_FLAG_MISSED_LCANCEL);
    int turn = Settings_Get(TM_SETTING_FLAG, TM_FLAG_RUN_TURNAROUND);
    for (int ply = 0; ply < 6; ++ply)
        for (int sub = 0; sub < 2; ++sub) {
            GOBJ *ft = Fighter_GetSubcharGObj(ply, sub);
            if (!ft) continue;
            FighterData *data = ft->userdata;
            CueFighter *entry = get_fighter(ft);
            if (!entry) continue;
            if (data->flags.dead) {
                entry->cue = (TMCueState){0}; entry->color = TM_CUE_NONE; entry->missed_entry = -1;
                entry->local_active = 0;
                continue;
            }
            int frozen = data->flags.hitlag || data->flags.freeze;
            if (data->state_id < ASID_LANDINGAIRN || data->state_id > ASID_LANDINGAIRLW)
                entry->missed_entry = -1;
            else if (entry->missed_entry < 0)
                entry->missed_entry = (u8)data->input.timer_trigger_any_ignore_hitlag >= (**stc_ftcommon).lcancel_input_window;
            int red = (turn && data->state_id == ASID_TURNRUN ? 2 : 0) |
                      (missed && entry->missed_entry > 0 ? 1 : 0);
            if (!missed && !turn) entry->cue.red = 0;
            entry->color = TMCue_Update(&entry->cue, data->state_id, data->state.frame, data->atk_instance,
                timing ? ActionCues_Remaining(ft) : -1, frozen, timing, red);
        }
}
static void Cue_Draw(GOBJ *gobj, int pass) {
    FighterData *data = gobj->userdata;
    CueFighter *entry = 0;
    for (unsigned i = 0; i < countof(fighters); ++i)
        if (fighters[i].object == gobj) { entry = &fighters[i]; break; }
    if (!entry || !entry->draw) return;
    unsigned cue = live && !data->flags.dead ? entry->color : TM_CUE_NONE;
    if ((cue == TM_CUE_YELLOW || cue == TM_CUE_GREEN) &&
        !Settings_Get(TM_SETTING_FLAG, TM_FLAG_LAST_BLOCKED_FRAME)) cue = TM_CUE_NONE;
    unsigned red_enabled = (Settings_Get(TM_SETTING_FLAG, TM_FLAG_MISSED_LCANCEL) ? 1 : 0) |
        (Settings_Get(TM_SETTING_FLAG, TM_FLAG_RUN_TURNAROUND) ? 2 : 0);
    if (cue == TM_CUE_RED && !(red_enabled & entry->cue.red_source)) cue = TM_CUE_NONE;
    /* Use current effective whole-body hurt status, not the action name or a
     * remaining-frame timer: script dodges and engine ledge/respawn protection
     * meet here. Read at draw time so pause, toggles and restores are immediate. */
    int protected = live && !data->flags.dead &&
        Settings_Get(TM_SETTING_FLAG, TM_FLAG_INVINCIBILITY) &&
        ActionCues_IsProtected(gobj);
    int local = live && !data->flags.dead && entry->local_active;
    if (!cue && !protected && !local) { entry->draw(gobj, pass); return; }
    /* Render-time composition: native/event colanim state survives byte-for-byte.
     * Timing explicitly replaces missed-cancel red, including Lab's own overlay. */
    ColorOverlay saved[3];
    memcpy(saved, data->color, sizeof(saved));
    data->color[0].color_enable = 0; data->color[2].color_enable = 0;
    static const GXColor colors[] = {{0,0,0,0}, {255,240,0,220}, {80,255,90,220}, {255,40,40,180}};
    GXColor color = colors[cue];
    if (!cue && !protected && local) color = entry->local_color;
    if (cue == TM_CUE_RED) color.a = TMCue_RedAlpha(entry->cue.red);
    if (protected && cue != TM_CUE_YELLOW && cue != TM_CUE_GREEN) {
        Playerblock *player = Fighter_GetPlayerblock((u8)data->ply);
        uint32_t rgba = TMTrail_PlayerColor(player ? player->color_accent : 4, !player || player->p_kind == 1);
        color = (GXColor){rgba >> 24, rgba >> 16, rgba >> 8, 112};
    }
    data->color[1].hex = color;
    data->color[1].color_enable = 1;
    entry->draw(gobj, pass);
    memcpy(data->color, saved, sizeof(saved));
}
void ActionCues_Clear(void) {
    for (unsigned i = 0; i < countof(fighters); ++i) {
        fighters[i].cue = (TMCueState){0};
        fighters[i].color = TM_CUE_NONE; fighters[i].missed_entry = -1;
        fighters[i].local_active = 0;
    }
    seen_frame = 0; /* Retain original draw callbacks while clearing tracking. */
}
void ActionCues_SceneChange(void) {
    live = 0; manager = 0;
    ActionCues_Clear(); /* Native scene cleanup owns fighters and the service GOBJ. */
}
void ActionCues_MatchStart(void) {
    if (manager) return;
    memset(fighters, 0, sizeof(fighters)); ActionCues_Clear(); live = 1;
    manager = GObj_Create(0, 7, 0);
    if (!manager) { live = 0; return; }
    GObj_AddProc(manager, Cue_Early, 0);
    GObj_AddProc(manager, Cue_Think, 23); /* After fighter/item/event and trail callbacks. */
}
