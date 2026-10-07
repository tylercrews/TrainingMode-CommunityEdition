#include "ledgedash.h"

static GXColor action_colors[] = {
    {40, 40, 40, 180},
    {120, 120, 120, 180},
    {128, 255, 128, 180},
    {80, 160, 80, 180},
    {52, 202, 228, 180},
    {230, 22, 198, 180},
    {255, 255, 255, 180},
    {255, 128, 128, 180},
    {128, 128, 255, 180},
};

enum menu_options
{
    OPT_POS,
    OPT_RESET,
    OPT_HUD,
    OPT_TIPS,
    OPT_CAM,
    OPT_INV,
    OPT_SPEED,
    OPT_OVERLAYS,
    OPT_RESETDELAY,
    OPT_LEDGE,
    OPT_CRITERION,
    OPT_PROTECTION,
    OPT_EGG,
    OPT_ABOUT,
    OPT_DEFAULTS,
    OPT_EXIT,
};

enum reset_mode
{
    OPTRESET_NONE,
    OPTRESET_SAME_SIDE,
    OPTRESET_SWAP,
    OPTRESET_SWAP_ON_SUCCESS,
    OPTRESET_RANDOM,
};

enum reset_pos
{
    OPTPOS_LEDGE,
    OPTPOS_FALLING,
    OPTPOS_STAGE,
    OPTPOS_RESPAWNPLAT,
    OPTPOS_RANDOM,  // Must be last
};

static char *panel_labels[5] = {"Angle", "GALINT", "Success Rate", "Criterion", "Egg"};
static char angle_text[24] = "-";
static char galint_text[24] = "-";
static char success_rate_text[48] = "-";
static char criterion_text[20] = "GALINT";
static char egg_text[24] = "Off";
static char *panel_info[5] = {angle_text, galint_text, success_rate_text, criterion_text, egg_text};
static void Ledgedash_ChangeLedge(GOBJ *menu, int value);
static void Ledgedash_ChangeCriterion(GOBJ *menu, int value);
static void Ledgedash_ChangeEgg(GOBJ *menu, int value);
static void Ledgedash_EggReset(LedgedashData *data, int reroll);
static void Ledgedash_EggThink(LedgedashData *data);
static void Ledgedash_EggCleanup(void);
static int Ledgedash_AttackDash(FighterData *data);
static void Ledgedash_UpdateRate(LedgedashData *data);
static void Ledgedash_ApplyOverlay(LedgedashData *data, GOBJ *fighter);
static void Ledgedash_UpdateAttempt(LedgedashData *data, FighterData *fighter);
static void Ledgedash_CleanupScene(void *unused);
static int Ledgedash_EggDamage(GOBJ *object);
static void Ledgedash_ChangeSavedSetting(GOBJ *menu, int value);
static void Ledgedash_ChangeTips(GOBJ *menu, int value);
static void Ledgedash_ResetSettings(GOBJ *menu);
typedef char ldsh_event_fits[(sizeof(LedgedashData) <= EVENT_DATASIZE) ? 1 : -1];
static struct {
    GOBJ *object, *retiring;
    int (*original_damage)(GOBJ *);
    int line, available, popped, prepared_ledge;
    float accumulated, fraction;
} egg_target;

// Main Menu
static const char *LdshOptions_CamMode[] = {"Normal", "Zoom", "Fixed", "Advanced"};
static const char *LdshOptions_Start[] = {"Ledge", "Falling", "Stage", "Respawn Platform", "Random"};
static float LdshOptions_GameSpeeds[] = {1.f, 5.f/6.f, 2.f/3.f, 1.f/2.f, 1.f/4.f};
static const char *LdshOptions_GameSpeedText[] = {"1", "5/6", "2/3", "1/2", "1/4"};
static const char *LdshOptions_Reset[] = {"None", "Same Side", "Swap", "Swap on Success", "Random"};
static const char *LdshOptions_ResetDelay[] = {"Slow", "Normal", "Fast", "Instant"};
static const char *LdshOptions_Ledge[] = {"Left", "Right"};
static const char *LdshOptions_Criteria[] = {"GALINT", "Waveland", "Attack/Dash in GALINT", "Pop Egg in GALINT"};
static const char *LdshCriterionLabels[] = {"GALINT", "Waveland", "Act GALINT", "Pop Egg"};
static const char *LdshOptions_Egg[] = {"Ground", "Platform", "Random"};
enum { EGG_ENABLE, EGG_TARGET, EGG_DISTANCE, EGG_RANDOM_DISTANCE, EGG_MIN_DISTANCE,
       EGG_MAX_DISTANCE, EGG_DAMAGE };
static EventOption LdshOptions_EggMenu[] = {
    {.kind = OPTKIND_TOGGLE, .name = "Enable Eggs",
     .desc = {"Place one ready-to-hit target each attempt.", "Pop Egg criteria requires this to be On."}, .OnChange = Ledgedash_ChangeEgg},
    {.kind = OPTKIND_STRING, .value_num = 3, .name = "Egg Target", .values = LdshOptions_Egg,
     .desc = {"Ground, Platform, or a random choice each attempt.", "Uses ground if no safe platform exists."}, .OnChange = Ledgedash_ChangeEgg},
    {.kind = OPTKIND_INT, .value_min = 5, .value_num = 76, .val = 20,
     .name = "Egg Distance", .format = "%d", .desc = {"Fixed distance inward from the ledge, in stage units."}, .OnChange = Ledgedash_ChangeEgg},
    {.kind = OPTKIND_TOGGLE, .name = "Randomize Distance",
     .desc = {"Choose a new distance between Min and Max each attempt."}, .OnChange = Ledgedash_ChangeEgg},
    {.kind = OPTKIND_INT, .value_min = 5, .value_num = 76, .val = 10,
     .name = "Min Distance", .format = "%d", .desc = {"Lower random distance bound. Reversed bounds are swapped."}, .OnChange = Ledgedash_ChangeEgg},
    {.kind = OPTKIND_INT, .value_min = 5, .value_num = 76, .val = 35,
     .name = "Max Distance", .format = "%d", .desc = {"Upper random distance bound, inclusive."}, .OnChange = Ledgedash_ChangeEgg},
    {.kind = OPTKIND_INT, .value_min = 1, .value_num = 50, .val = 10,
     .name = "Egg Pop Damage", .format = "%d", .desc = {"Accumulated damage needed to pop the target egg."}, .OnChange = Ledgedash_ChangeEgg},
};
static EventMenu LdshMenu_Eggs = {.name = "Egg Targets", .option_num = countof(LdshOptions_EggMenu),
    .options = LdshOptions_EggMenu};
static int LdshOptions_ResetDelaySuccess[] = { 120, 60, 30, 1 };
static int LdshOptions_ResetDelayFailure[] = { 60, 20, 1, 1 };

static EventOption LdshOptions_Main[] = {
    {
        .kind = OPTKIND_STRING,
        .value_num = sizeof(LdshOptions_Start) / 4,
        .name = "Starting Position",
        .desc = {"Choose where the fighter is placed ",
                 "after resetting positions."},
        .values = LdshOptions_Start,
        .OnChange = Ledgedash_ToggleStartPosition,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = sizeof(LdshOptions_Reset) / 4,
        .val = TM_LEDGE_DEFAULT_RESET,
        .name = "Reset",
        .desc = {"Change where the fighter gets placed",
                 "after a ledgedash attempt."},
        .values = LdshOptions_Reset,
        .OnChange = Ledgedash_ChangeSavedSetting,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "HUD",
        .desc = {"Toggle visibility of the HUD."},
        .val = 1,
        .OnChange = Ledgedash_ChangeShowHUD,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Tips",
        .desc = {"Toggle the onscreen display of tips."},
        .val = TM_LEDGE_DEFAULT_TIPS,
        .OnChange = Ledgedash_ChangeTips,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = sizeof(LdshOptions_CamMode) / 4,
        .name = "Camera Mode",
        .desc = {"Adjust the camera's behavior.",
                 "In advanced mode, use C-Stick while holding",
                 "A/B/Y to pan, rotate and zoom, respectively."},
        .values = LdshOptions_CamMode,
        .OnChange = Ledgedash_ChangeCamMode,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Keep Ledge Invincibility",
        .desc = {"Keep maximum invincibility while on the ledge",
                 "to practice the ledgedash inputs."},
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = sizeof(LdshOptions_GameSpeedText) /
                     sizeof(*LdshOptions_GameSpeedText),
        .name = "Game Speed",
        .desc = {"Change how fast the game engine runs."},
        .values = LdshOptions_GameSpeedText,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Color Overlays",
        .desc = {"Show which state you are in with a color overlay."},
    },
    {
        .kind = OPTKIND_STRING,
        .value_num =
            sizeof(LdshOptions_ResetDelay) / sizeof(*LdshOptions_ResetDelay),
        .val = TM_LEDGE_DEFAULT_DELAY,
        .name = "Reset Delay",
        .desc = {"Change how quickly you can start a new ledgedash."},
        .values = LdshOptions_ResetDelay,
        .OnChange = Ledgedash_ChangeSavedSetting,
    },
    {
        .kind = OPTKIND_STRING, .value_num = 2, .name = "Ledge",
        .desc = {"Choose the left or right ledge. D-pad left/right also work."},
        .values = LdshOptions_Ledge, .OnChange = Ledgedash_ChangeLedge,
    },
    {
        .kind = OPTKIND_STRING, .value_num = LDSH_CRITERIA_COUNT, .name = "Success Criteria",
        .desc = {"Choose what counts as a successful ledgedash.", "Changing this resets session statistics and the attempt.", "Pop Egg requires popping the target during grounded GALINT."},
        .values = LdshOptions_Criteria, .OnChange = Ledgedash_ChangeCriterion,
    },
    {
        .kind = OPTKIND_TOGGLE, .val = 1, .name = "Protection Highlight",
        .desc = {"Brighten the action color while invincible/intangible.", "Works independently of Color Overlays."},
    },
    {
        .kind = OPTKIND_MENU, .name = "Egg Targets", .menu = &LdshMenu_Eggs,
        .desc = {"Enable eggs and choose target, distance, randomization and damage."},
    },
    {
        .kind = OPTKIND_INFO,
        .name = "HELP",
        .desc =
            {"Ledgedashing is the act of wavedashing onto stage from ledge.",
             "This is most commonly done by dropping off ledge, double jumping ",
             "immediately, and quickly airdodging onto stage. Each input",
             "is performed quickly after the last, making it difficult and risky."},
    },
    {
        .kind = OPTKIND_FUNC, .name = "Reset Event Settings",
        .desc = {"Restore this event's defaults, including saved choices.", "Global Settings and other events are unchanged."},
        .OnSelect = Ledgedash_ResetSettings,
    },
    {
        .kind = OPTKIND_FUNC,
        .name = "Exit",
        .desc = {"Return to the Event Selection Screen."},
        .OnSelect = Event_Exit,
    },
};

static EventOption Ldsh_FrameAdvance = {
    .kind = OPTKIND_TOGGLE,
    .name = "Frame Advance",
    .desc = {"Enable frame advance. Press to advance one",
             "frame. Hold to advance at normal speed."},
};

static Shortcut Ldsh_Shortcuts[] = {
    {
        .button_mask = HSD_BUTTON_A,
        .option = &Ldsh_FrameAdvance,
    }
};

static ShortcutList Ldsh_ShortcutList = {
    .count = countof(Ldsh_Shortcuts),
    .list = Ldsh_Shortcuts,
};

static EventMenu LdshMenu_Main = {
    .name = "Ledgedash Training",
    .option_num = sizeof(LdshOptions_Main) / sizeof(EventOption),
    .options = LdshOptions_Main,
    .shortcuts = &Ldsh_ShortcutList,
};

static const u8 saved_ledge_rows[TM_LEDGE_PREF_COUNT] = {
    OPT_POS, OPT_RESET, OPT_CRITERION, OPT_RESETDELAY, OPT_TIPS,
};
static void Ledgedash_LoadSettings(void) {
    for (unsigned i = 0; i < TM_LEDGE_PREF_COUNT; ++i) {
        EventOption *option = &LdshOptions_Main[saved_ledge_rows[i]];
        option->val = option->val_prev = TM_GetSetting(TM_SETTING_LEDGEDASH, i);
    }
    if (LdshOptions_Main[OPT_CRITERION].val == LDSH_POP_EGG) LdshOptions_EggMenu[EGG_ENABLE].val = 1;
    sprintf(criterion_text, "%s", LdshCriterionLabels[LdshOptions_Main[OPT_CRITERION].val]);
}
static void Ledgedash_ChangeSavedSetting(GOBJ *menu, int value) {
    MenuData *data = menu->userdata;
    if (data->curr_menu != &LdshMenu_Main) return;
    unsigned row = data->curr_menu->scroll + data->curr_menu->cursor;
    for (unsigned i = 0; i < TM_LEDGE_PREF_COUNT; ++i)
        if (saved_ledge_rows[i] == row) TM_SetSetting(TM_SETTING_LEDGEDASH, i, value);
}
static void Ledgedash_ChangeTips(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_LEDGEDASH, TM_LEDGE_TIPS, value);
    Tips_Toggle(menu, value);
}
static void Ledgedash_ResetSettings(GOBJ *menu) {
    /* Defaults for the event-local choices too; callbacks run once after the batch. */
    static const u8 main_defaults[OPT_EGG] = {
        [OPT_RESET] = TM_LEDGE_DEFAULT_RESET, [OPT_HUD] = 1, [OPT_TIPS] = TM_LEDGE_DEFAULT_TIPS,
        [OPT_RESETDELAY] = TM_LEDGE_DEFAULT_DELAY, [OPT_PROTECTION] = 1,
    };
    static const u8 egg_defaults[EGG_DAMAGE + 1] = {
        [EGG_DISTANCE] = 20, [EGG_MIN_DISTANCE] = 10, [EGG_MAX_DISTANCE] = 35, [EGG_DAMAGE] = 10,
    };
    int old_criterion = LdshOptions_Main[OPT_CRITERION].val;
    for (unsigned i = 0; i < countof(main_defaults); ++i)
        LdshOptions_Main[i].val = LdshOptions_Main[i].val_prev = main_defaults[i];
    for (unsigned i = 0; i < countof(egg_defaults); ++i)
        LdshOptions_EggMenu[i].val = LdshOptions_EggMenu[i].val_prev = egg_defaults[i];
    TM_SetSetting(TM_SETTING_EVENT_RESET, TM_EVENT_LEDGEDASH, 1);
    Ledgedash_LoadSettings();
    LedgedashData *data = event_vars->event_gobj->userdata;
    data->ledge = -1;
    if (old_criterion != LDSH_GALINT) {
        data->hud.total_count = data->hud.successful_count = 0;
        Ledgedash_UpdateRate(data);
    }
    Ledgedash_ChangeShowHUD(menu, 1);
    Ledgedash_ChangeCamMode(menu, 0);
    Tips_Toggle(menu, TM_LEDGE_DEFAULT_TIPS);
    Fighter_PlaceOnLedge();
}

// Init Function
void Event_Init(GOBJ *gobj)
{
    Ledgedash_LoadSettings();
    LedgedashData *event_data = gobj->userdata;
    Ledgedash_EggCleanup();
    GOBJ *cleanup = GObj_Create(0, 0, 0);
    if (cleanup) GObj_AddUserData(cleanup, 0, Ledgedash_CleanupScene, &egg_target);

    HSD_Update *hsd_update = stc_hsd_update;
    hsd_update->checkPause = Update_CheckPause;
    hsd_update->checkAdvance = Update_CheckAdvance;

    // standardize camera
    float *unk_cam = (void *)0x803bcca0;
    stc_stage->fov_r = 0; // no camera rotation
    stc_stage->x28 = 1;   // pan value?
    stc_stage->x2c = 1;   // pan value?
    stc_stage->x30 = 1;   // pan value?
    stc_stage->x34 = 130; // zoom out
    unk_cam[0x40 / 4] = 30;

    // Init hitlog
    event_data->hitlog_gobj = Ledgedash_HitLogInit();

    // Init Fighter
    Ledgedash_FtInit(event_data);

    Fighter_PlaceOnLedge();
}
// Think Function
void Event_Think(GOBJ *event)
{
    LedgedashData *event_data = event->userdata;

    // get fighter data
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;

    // no ledgefall
    FtCliffCatch *ft_state = (void *)&hmn_data->state_var;
    if (hmn_data->state_id == ASID_CLIFFWAIT)
        ft_state->fall_timer = 2;

    if (LdshOptions_Main[OPT_INV].val == 1) {
        if (hmn_data->state_id == ASID_CLIFFWAIT) {
            hmn_data->hurt.intang_frames.ledge = 30;
            hmn_data->TM.state_frame = 1;
        }
    }

    if (hmn_data->input.down & HSD_BUTTON_DPAD_LEFT) {
        event_data->ledge = -1;
        Fighter_PlaceOnLedge();
    } else if (hmn_data->input.down & HSD_BUTTON_DPAD_RIGHT) {
        event_data->ledge = 1;
        Fighter_PlaceOnLedge();
    }

    unsigned serial = event_vars->get_restore_serial();
    if (serial != event_data->restore_serial) {
        event_data->restore_serial = serial;
        Ledgedash_EggReset(event_data, 0);
        event_data->seen_frame = 0;
    }
    unsigned frame = stc_match->time_frames;
    if (!event_data->seen_frame || frame != event_data->native_frame) {
        event_data->seen_frame = 1; event_data->native_frame = frame;
        Ledgedash_EggThink(event_data);
        Ledgedash_ResetThink(event_data, hmn);
        Ledgedash_HUDThink(event_data, hmn_data);
        Ledgedash_HitLogThink(event_data, hmn);
    }

    Ledgedash_ApplyOverlay(event_data, hmn);
}
void Event_Exit(GOBJ *menu)
{
    Memcard_SaveIfChanged();
    Ledgedash_EggCleanup();
    event_vars->set_local_overlay(Fighter_GetGObj(0), 0);
    // end game
    stc_match->state = 3;

    // cleanup
    Match_EndVS();
}

void Ledgedash_HUDThink(LedgedashData *event_data, FighterData *hmn_data)
{
    // run tip logic
    Tips_Think(event_data, hmn_data);

    // check to initialize timer
    if (hmn_data->state_id == ASID_CLIFFWAIT &&
        (!event_data->action_state.is_ledgegrab || event_data->action_state.is_release))
    {
        int prepared = egg_target.prepared_ledge;
        Ledgedash_InitVariables(event_data);
        event_data->reset_timer = 0;
        if (!prepared) Ledgedash_EggReset(event_data, 0);
        egg_target.prepared_ledge = 0; // Do not reroll a target already placed by this reset.
        event_data->tip.refresh_num++;
    }

    u32 curr_frame = event_data->action_state.timer++;
    int hud_updating =
        curr_frame < countof(event_data->action_state.action_log)
        && hmn_data->hurt.intang_frames.ledge != 0;

    if (hud_updating) {
        int state_id = hmn_data->state_id;
        int action = LDACT_GALINT;

        // look for cliffwait
        if (state_id == ASID_CLIFFWAIT)
             action = LDACT_CLIFFWAIT;

        // look for release
        else if (state_id == ASID_FALL)
            action = hmn_data->flags.is_fastfall ? LDACT_FASTFALL : LDACT_FALL;

        // look for jump
        else if (
            state_id == ASID_JUMPAERIALF
            || state_id == ASID_JUMPAERIALB
            // check for kirby and jiggs jump
            || ((hmn_data->kind == 4 || hmn_data->kind == 15) && (state_id >= 341 && state_id <= 345)))
            action = LDACT_JUMP;

        // look for airdodge
        else if (state_id == ASID_ESCAPEAIR)
            action = LDACT_AIRDODGE;

        // look for attack
        else if (Ledgedash_AttackDash(hmn_data))
            action = LDACT_ATTACK_DASH;

        // look for landing
        else if (
            (state_id == ASID_LANDING && hmn_data->TM.state_frame < hmn_data->attr.normal_landing_lag)
            || state_id == ASID_LANDINGFALLSPECIAL
        )
            action = LDACT_LANDING;

        // look for ledge options
        else if (ASID_CLIFFCLIMBSLOW <= state_id && state_id <= ASID_CLIFFJUMPQUICK2)
            action = LDACT_NONE;

        event_data->action_state.action_log[curr_frame] = action;
    }

    // grab airdodge angle
    if (event_data->action_state.is_airdodge == 0)
    {
        if ((hmn_data->state_id == ASID_ESCAPEAIR) || (hmn_data->TM.state_prev[0] == ASID_ESCAPEAIR))
        {
            // determine airdodge angle
            float angle = atan2(fabs(hmn_data->input.lstick.Y), fabs(hmn_data->input.lstick.X));

            // save airdodge angle
            event_data->hud.airdodge_angle = angle;
            event_data->action_state.is_airdodge = 1;
        }
    }

    if (hmn_data->state_id == ASID_CLIFFWAIT)
        event_data->action_state.is_ledgegrab = 1;
    if (event_data->action_state.is_ledgegrab && hmn_data->state_id != ASID_CLIFFWAIT && hmn_data->state_id != ASID_CLIFFCATCH)
        event_data->action_state.is_release = 1;

    Ledgedash_UpdateAttempt(event_data, hmn_data);

}

void Ledgedash_ResetThink(LedgedashData *event_data, GOBJ *hmn)
{
    FighterData *hmn_data = hmn->userdata;

    int reset_mode = LdshOptions_Main[OPT_RESET].val;

    if (reset_mode == OPTRESET_NONE)
        return;

    if (event_data->reset_timer > 0) {
        event_data->reset_timer--;
        
        if (event_data->reset_timer > 0)
            return;

        int swap = false;
        switch (reset_mode) {
            case OPTRESET_SAME_SIDE:
                break;
            case OPTRESET_SWAP:
                swap = true;
                break;
            case OPTRESET_SWAP_ON_SUCCESS:
                swap = event_data->was_successful;
                break;
            case OPTRESET_RANDOM:
                swap = HSD_Randi(2);
                break;
        }

        if (swap)
            event_data->ledge = -event_data->ledge;

        Fighter_PlaceOnLedge();
    } else if (event_data->action_state.is_finished) {
        
        int reset_idx = LdshOptions_Main[OPT_RESETDELAY].val;
        event_data->reset_timer = LdshOptions_ResetDelaySuccess[reset_idx];
    } else {
        /* Original reset checks and failure delay, including the frame-9
         * airdodge check during a Falling start. Hard criteria alone keep their
         * grounded GALINT opportunity open until the verdict is resolved. */
        int pending_hard = event_data->attempt.phase == LDSH_LANDED &&
            LdshOptions_Main[OPT_CRITERION].val >= LDSH_ACT_GALINT && hmn_data->hurt.intang_frames.ledge > 0;
        if (Ldsh_LegacyResetFailure(hmn_data->state_id, hmn_data->TM.state_frame,
                hmn_data->flags.dead, !hmn_data->phys.air_state,
                event_data->action_state.is_release, pending_hard)) {
            int reset_idx = LdshOptions_Main[OPT_RESETDELAY].val;
            event_data->reset_timer = LdshOptions_ResetDelayFailure[reset_idx];
            event_data->was_successful = false;
            if (!event_data->attempt.counted) {
                event_data->attempt.phase = LDSH_RESOLVED;
                event_data->attempt.result = LDSH_FAILURE;
                event_data->attempt.counted = 1;
                if (event_data->hud.total_count < 999999) event_data->hud.total_count++;
                Ledgedash_UpdateRate(event_data);
                SFX_PlayCommon(3);
            }
        }
    }

}
void Ledgedash_InitVariables(LedgedashData *event_data)
{
    LdshAttempt_Reset(&event_data->attempt);
    event_data->action_state.timer = 0;
    event_data->action_state.is_ledgegrab = 0;
    event_data->action_state.is_release = 0;
    event_data->action_state.is_airdodge = 0;
    event_data->action_state.is_finished = 0;

    // init action log
    for (u32 i = 0; i < countof(event_data->action_state.action_log); i++)
    {
        event_data->action_state.action_log[i] = 0;
    }
}

// Menu Toggle functions
void Ledgedash_ToggleStartPosition(GOBJ *menu_gobj, int value)
{
    TM_SetSetting(TM_SETTING_LEDGEDASH, TM_LEDGE_START, value);
    Fighter_PlaceOnLedge();
}

// Hitlog functions
GOBJ *Ledgedash_HitLogInit(void)
{

    GOBJ *hit_gobj = GObj_Create(0, 0, 0);
    LdshHitlogData *hit_data = calloc(sizeof(LdshHitlogData));
    GObj_AddUserData(hit_gobj, 4, HSD_Free, hit_data);
    GObj_AddGXLink(hit_gobj, Ledgedash_HitLogGX, 5, 0);

    // init array
    hit_data->num = 0;

    return hit_gobj;
}
void Ledgedash_HitLogThink(LedgedashData *event_data, GOBJ *hmn)
{
    FighterData *hmn_data = hmn->userdata;
    LdshHitlogData *hitlog_data = event_data->hitlog_gobj->userdata;

    // log hitboxes
    if (event_data->action_state.is_finished && hmn_data->hurt.intang_frames.ledge > 0)
    {

        // iterate through fighter hitboxes
        for (u32 i = 0; i < countof(hmn_data->hitbox); i++)
        {

            ftHit *this_hit = &hmn_data->hitbox[i];

            if ((this_hit->active != 0) &&           // if hitbox is active
                (hitlog_data->num < LDSH_HITBOXNUM)) // if not over max
            {

                // log info
                LdshHitboxData *this_ldsh_hit = &hitlog_data->hitlog[hitlog_data->num];
                this_ldsh_hit->size = this_hit->size;
                this_ldsh_hit->pos_curr = this_hit->pos;
                this_ldsh_hit->pos_prev = this_hit->pos_prev;
                this_ldsh_hit->kind = this_hit->attribute;

                // increment hitboxes
                hitlog_data->num++;
            }
        }

        // iterate through items belonging to fighter
        GOBJ *this_item = (*stc_gobj_lookup)[MATCHPLINK_ITEM];
        while (this_item != 0)
        {
            ItemData *this_itemdata = this_item->userdata;

            // ensure belongs to the fighter
            if (this_itemdata->fighter_gobj == hmn)
            {
                // iterate through item hitboxes
                for (u32 i = 0; i < countof(hmn_data->hitbox); i++)
                {

                    itHit *this_hit = &this_itemdata->hitbox[i];

                    if ((this_hit->active != 0) &&           // if hitbox is active
                        (hitlog_data->num < LDSH_HITBOXNUM)) // if not over max
                    {

                        // log info
                        LdshHitboxData *this_ldsh_hit = &hitlog_data->hitlog[hitlog_data->num];
                        this_ldsh_hit->size = this_hit->size;
                        this_ldsh_hit->pos_curr = this_hit->pos;
                        this_ldsh_hit->pos_prev = this_hit->pos_prev;
                        this_ldsh_hit->kind = this_hit->attribute;

                        // increment hitboxes
                        hitlog_data->num++;
                    }
                }
            }

            this_item = this_item->next;
        }
    }
}

void Ledgedash_HitLogGX(GOBJ *gobj, int pass)
{
    if (pass != 2) return;

    static GXColor hitlog_ambient = {128, 0, 0, 150};
    static GXColor hit_diffuse = {255, 99, 99, 150};
    static GXColor grab_diffuse = {255, 0, 255, 150};
    static GXColor detect_diffuse = {255, 255, 255, 150};

    LdshHitlogData *hitlog_data = gobj->userdata;
    
    // panel info
    event_vars->HUD_DrawInfoPanel((const char**)panel_labels, (const char**)panel_info,
        LdshOptions_EggMenu[EGG_ENABLE].val ? countof(panel_labels) : countof(panel_labels) - 1);

    // action log
    LedgedashData *event_data = event_vars->event_gobj->userdata;
    static char *names[] = {
        "Cliffwait",
        "Fall",
        "Fastfall",
        "Jump",
        "Airdodge",
        "Act",
        "Landing",
        "GALINT",
    };
    event_vars->HUD_DrawActionLogBar(
        event_data->action_state.action_log,
        action_colors,
        countof(event_data->action_state.action_log)
    );
    event_vars->HUD_DrawActionLogKey(
        names,
        &action_colors[1],
        countof(names)
    );
    
    // hitboxes
    for (int i = 0; i < hitlog_data->num; i++)
    {
        LdshHitboxData *this_ldsh_hit = &hitlog_data->hitlog[i];

        // determine color
        GXColor *diffuse;
        if (this_ldsh_hit->kind == 0)
            diffuse = &hit_diffuse;
        else if (this_ldsh_hit->kind == 8)
            diffuse = &grab_diffuse;
        else if (this_ldsh_hit->kind == 11)
            diffuse = &detect_diffuse;
        else
            diffuse = &hit_diffuse;

        Develop_DrawSphere(this_ldsh_hit->size, &this_ldsh_hit->pos_curr, &this_ldsh_hit->pos_prev, diffuse, &hitlog_ambient);
    }
}

// Fighter fuctions
void Ledgedash_FtInit(LedgedashData *event_data)
{
    // create camera box
    CmSubject *cam = CameraSubject_Alloc();
    cam->boundleft_proj = -10;
    cam->boundright_proj = 10;
    cam->boundtop_proj = 10;
    cam->boundbottom_proj = -10;
    cam->boundleft_curr = cam->boundleft_proj;
    cam->boundright_curr = cam->boundright_proj;
    cam->boundtop_curr = cam->boundtop_proj;
    cam->boundbottom_curr = cam->boundbottom_proj;
    event_data->cam = cam;
    event_data->was_successful = false;
    event_data->reset_timer = 0;
    event_data->ledge = -1; // start on left ledge

    //if (event_vars->ledge_l == -1 || event_vars->ledge_r == -1) {
    //    event_data->cam->is_disable = 0;
    //    event_vars->Tip_Display(500 * 60, "Error:\nIt appears there are no\ngood ledges on this stage...");
    //}
}

void Ledgedash_ChangeShowHUD(GOBJ *menu_gobj, int show) {
    HUDCamData *cam = event_vars->hudcam_gobj->userdata;
    cam->hide = !show;
}

void Ledgedash_ChangeCamMode(GOBJ *menu_gobj, int value)
{
    MatchCamera *cam = stc_matchcam;

    // normal cam
    if (value == 0)
    {
        Match_SetNormalCamera();
    }
    // zoom cam
    else if (value == 1)
    {
        Match_SetFreeCamera(0, 3);
        cam->freecam_fov.X = 140;
        cam->freecam_rotate.Y = 10;
    }
    // fixed
    else if (value == 2)
    {
        Match_SetFixedCamera();
    }
    else if (value == 3)
    {
        Match_SetDevelopCamera();
    }
    Match_CorrectCamera();
}

void Event_Update(void)
{
    if (Pause_CheckStatus(1) != 2) {
        float speed = LdshOptions_GameSpeeds[LdshOptions_Main[OPT_SPEED].val];
        HSD_SetSpeedEasy(speed);
    } else {
        HSD_SetSpeedEasy(1.0);
    }
}

int Ledge_Find(int search_dir, float xpos_start, float *ledge_dir)
{
    // get line and vert pointers
    CollLine *collline = *stc_collline;

    // get initial closest
    float xpos_closest;
    if (search_dir == -1) // search left
        xpos_closest = -5000;
    else if (search_dir == 1) // search right
        xpos_closest = 5000;
    else // search both
        xpos_closest = 5000;

    // look for the closest ledge
    int index_closest = -1;
    CollGroup *this_group = *stc_firstcollgroup;
    while (this_group != 0) // loop through ground links
    {

        // 2 passes, one for ground and one for dynamic lines
        int line_index, line_num;
        for (int i = 0; i < 2; i++)
        {
            // first pass, use floors
            if (i == 0)
            {
                line_index = this_group->desc->floor_start;          // first ground link
                line_num = line_index + this_group->desc->floor_num; // ground link num
            }
            // second pass, use dynamics
            else if (i == 1)
            {
                line_index = this_group->desc->dyn_start;          // first ground link
                line_num = line_index + this_group->desc->dyn_num; // ground link num
            }

            // loop through lines
            while (line_index < line_num)
            {
                // get all data for this line
                CollLine *this_line = &collline[line_index]; // ??? i actually dont know why i cant access this directly
                CollLineDesc *this_linedesc = this_line->desc;

                // check if this link is a ledge
                if (this_linedesc->is_ledge)
                {

                    // check both sides of this ledge
                    Vec3 ledge_pos;
                    for (int j = 0; j < 2; j++)
                    {
                        // first pass, check left
                        if (j == 0)
                        {
                            GrColl_GetGroundLineEndLeft(line_index, &ledge_pos);
                        }
                        else if (j == 1)
                        {
                            GrColl_GetGroundLineEndRight(line_index, &ledge_pos);
                        }

                        // is within the camera range
                        if ((ledge_pos.X > Stage_GetCameraLeft()) && (ledge_pos.X < Stage_GetCameraRight()) && (ledge_pos.Y > Stage_GetCameraBottom()) && (ledge_pos.Y < Stage_GetCameraTop()))
                        {

                            // check for any obstructions
                            float dir_mult;
                            if (j == 0) // left ledge
                                dir_mult = -1;
                            else if (j == 1) // right ledge
                                dir_mult = 1;
                            int ray_index;
                            int ray_kind;
                            Vec3 ray_angle;
                            Vec3 ray_pos;
                            float from_x = ledge_pos.X + (2 * dir_mult);
                            float to_x = from_x;
                            float from_y = ledge_pos.Y + 5;
                            float to_y = from_y - 10;
                            int is_ground = GrColl_RaycastGround(&ray_pos, &ray_index, &ray_kind, &ray_angle, -1, -1, -1, 0, from_x, from_y, to_x, to_y, 0);
                            if (is_ground == 0)
                            {
                                int is_closer = 0;

                                if (search_dir == -1) // check if to the left
                                {
                                    if ((ledge_pos.X > xpos_closest) && (ledge_pos.X < xpos_start))
                                        is_closer = 1;
                                }
                                else if (search_dir == 1) // check if to the right
                                {
                                    if ((ledge_pos.X < xpos_closest) && (ledge_pos.X > xpos_start))
                                        is_closer = 1;
                                }
                                else // check if any direction
                                {
                                    float dist_old = fabs(xpos_start - xpos_closest);
                                    float dist_new = fabs(xpos_start - ledge_pos.X);
                                    if (dist_new < dist_old)
                                        is_closer = 1;
                                }

                                // determine direction
                                if (is_closer)
                                {

                                    // now determine if this line is a ledge in this direction
                                    if (j == 0) // left ledge
                                    {
                                        CollLine *prev_line = &collline[this_linedesc->line_prev];          // ??? i actually dont know why i cant access this directly
                                        if ((this_linedesc->line_prev == -1) || (prev_line->is_rwall == 1)) // if prev line is a right wall / if prev line doesnt exist
                                        {

                                            // save info on this line
                                            xpos_closest = ledge_pos.X; // save left vert's X position
                                            index_closest = line_index;
                                            *ledge_dir = 1;
                                        }
                                    }
                                    else if (j == 1) // right ledge
                                    {
                                        CollLine *next_line = &collline[this_linedesc->line_next];          // ??? i actually dont know why i cant access this directly
                                        if ((this_linedesc->line_next == -1) || (next_line->is_lwall == 1)) // if prev line is a right wall / if prev line doesnt exist
                                        {

                                            // save info on this line
                                            xpos_closest = ledge_pos.X; // save left vert's X position
                                            index_closest = line_index;
                                            *ledge_dir = -1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                line_index++;
            }
        }

        // get next
        this_group = this_group->next;
    }

    return index_closest;
}
void Fighter_PlaceOnLedge(void)
{
    LedgedashData *event_data = event_vars->event_gobj->userdata;
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;

    float ledge_dir;
    int line_index = Ledge_Find(event_data->ledge, 0.f, &ledge_dir);

    if (line_index == -1) {
        event_vars->Tip_Display(500 * 60, "Error:\nIt appears there are no \nledges on this stage..");
        return;
    }

    Ledgedash_EggCleanup();
    Ledgedash_InitVariables(event_data);
    event_data->tip.refresh_displayed = 0;
    event_vars->trails->clear();
    event_vars->clear_action_cues();
    event_data->tip.is_input_release = 0;
    event_data->tip.refresh_num = 0;

    // get ledge position
    Vec3 ledge_pos;
    if (ledge_dir > 0)
        GrColl_GetGroundLineEndLeft(line_index, &ledge_pos);
    else
        GrColl_GetGroundLineEndRight(line_index, &ledge_pos);

    event_data->ledge_pos = ledge_pos;
    event_data->ledge_dir = ledge_dir;
    event_data->ledge_line = line_index;
    LdshOptions_Main[OPT_LEDGE].val = event_data->ledge > 0;

    // remove velocity
    hmn_data->phys.self_vel.X = 0;
    hmn_data->phys.self_vel.Y = 0;

    // restore tether
    hmn_data->flags.used_tether = 0;


    // Sleep first
    Fighter_EnterSleep(hmn, 0);
    Fighter_EnterRebirth(hmn);

    // face the ledge
    hmn_data->facing_direction = ledge_dir;

    // check starting position
    int start_pos = LdshOptions_Main[OPT_POS].val;

SWITCH_START_POS:
    switch (start_pos)
    {
    case OPTPOS_RANDOM:
    {
        start_pos = HSD_Randi(OPTPOS_RANDOM);
        goto SWITCH_START_POS;
    }
    case OPTPOS_LEDGE:
    {
        // place player on this ledge
        event_data->tip.refresh_num = -1; // setting this to -1 because the per frame code will add 1 and make it 0
        FtCliffCatch *ft_state = (void *)&hmn_data->state_var;
        ft_state->ledge_index = line_index; // store line index
        Fighter_EnterCliffWait(hmn);
        ft_state->timer = 0; // spoof as on ledge for a frame already
        Fighter_SetAirborne(hmn_data);
        Fighter_EnableCollUpdate(hmn_data);
        Coll_CheckLedge(&hmn_data->coll_data);
        Fighter_MoveToCliff(hmn);
        Fighter_UpdatePosition(hmn);
        ftCommonData *ftcommon = *stc_ftcommon;

        // This needs to be +1 for some reason, otherwise we get an off-by-one in galint calculations
        Fighter_ApplyIntang(hmn, ftcommon->cliff_invuln_time+1);
        break;
    }
    case OPTPOS_FALLING:
    {
        // place player falling above the ledge
        hmn_data->phys.pos.X = ledge_pos.X + -5 * ledge_dir; // slight nudge to prevent accidentally landing on stage
        hmn_data->phys.pos.Y = ledge_pos.Y + 20;
        Fighter_UpdatePosition(hmn);
        Fighter_EnterFall(hmn);
        break;
    }
    case OPTPOS_STAGE:
    {
        // place player on stage next to ledge
        Vec3 coll_pos, line_unk;
        int line_index, line_kind;
        float x = ledge_pos.X + 12 * ledge_dir;
        float from_y = ledge_pos.Y + 5;
        float to_y = from_y - 10;
        int is_ground = GrColl_RaycastGround(&coll_pos, &line_index, &line_kind, &line_unk, -1, -1, -1, 0, x, from_y, x, to_y, 0);
        if (is_ground == 1)
        {
            hmn_data->phys.pos = coll_pos;
            Fighter_UpdatePosition(hmn);
            Fighter_EnterWait(hmn);
        }
        break;
    }
    case OPTPOS_RESPAWNPLAT:
    {
        // place player in a random position in respawn wait
        float xpos_min = 40;
        float xpos_max = 50;
        float ypos_min = -30;
        float ypos_max = 30;
        hmn_data->phys.pos.X = ((ledge_dir * -1) * (xpos_min + HSD_Randi(xpos_max - xpos_min) + HSD_Randf())) + (ledge_pos.X);
        hmn_data->phys.pos.Y = ((ledge_dir * -1) * (ypos_min + HSD_Randi(ypos_max - ypos_min) + HSD_Randf())) + (ledge_pos.Y);

        // enter rebirth
        Fighter_EnterRebirthWait(hmn);
        hmn_data->cb.Phys = RebirthWait_Phys;
        hmn_data->cb.IASA = RebirthWait_IASA;

        Fighter_UpdateRebirthPlatformPos(hmn);

        break;
    }
    }

    event_data->resolved_start = start_pos;
    event_data->restore_serial = event_vars->get_restore_serial();
    event_data->seen_frame = 0;

    // avoid double reset in case user manually resets
    event_data->reset_timer = 0;

    // update camera box
    CmSubject *cam = event_data->cam;
    cam->cam_pos.X = ledge_pos.X + (ledge_dir * 20);
    cam->cam_pos.Y = ledge_pos.Y + 15;

    Fighter_UpdateCamera(hmn);
    
    // Prevent double reset caused by normal getup -> ledge jump combo with an instant reset delay.
    hmn_data->input.timer_lstick_tilt_y = 10;

    // remove all particles
    for (int i = 0; i < PTCL_LINKMAX; i++)
    {
        Particle **ptcls = &stc_ptcl[i];
        Particle *ptcl = *ptcls;
        while (ptcl != 0)
        {

            Particle *ptcl_next = ptcl->next;

            // begin destroying this particle

            // subtract some value, 8039c9f0
            if (ptcl->gen != 0)
            {
                ptcl->gen->particle_num--;
            }
            // remove from generator? 8039ca14
            if (ptcl->gen != 0)
                psRemoveParticleAppSRT(ptcl);

            // delete parent jobj, 8039ca48
            psDeletePntJObjwithParticle(ptcl);

            // update most recent ptcl pointer
            *ptcls = ptcl->next;

            // free alloc, 8039ca54
            HSD_ObjFree((void *)0x804d0f60, ptcl);

            // decrement ptcl total
            u16 ptclnum = *stc_ptclnum;
            ptclnum--;
            *stc_ptclnum = ptclnum;

            // get next
            ptcl = ptcl_next;
        }
    }

    // remove all camera shake gobjs (p_link 18, entity_class 3)
    GOBJ *gobj = (*stc_gobj_lookup)[MATCHPLINK_MATCHCAM];
    while (gobj != 0)
    {

        GOBJ *gobj_next = gobj->next;

        // if entity class 3 (quake)
        if (gobj->entity_class == 3)
        {
            GObj_Destroy(gobj);
        }

        gobj = gobj_next;
    }
    Ledgedash_EggReset(event_data, 1);
    egg_target.prepared_ledge = hmn_data->state_id == ASID_CLIFFWAIT;
}
void Fighter_UpdatePosition(GOBJ *fighter)
{

    FighterData *fighter_data = fighter->userdata;

    // Update Position (Copy Physics XYZ into all ECB XYZ)
    fighter_data->coll_data.topN_Curr.X = fighter_data->phys.pos.X;
    fighter_data->coll_data.topN_Curr.Y = fighter_data->phys.pos.Y;
    fighter_data->coll_data.topN_Prev.X = fighter_data->phys.pos.X;
    fighter_data->coll_data.topN_Prev.Y = fighter_data->phys.pos.Y;
    fighter_data->coll_data.topN_CurrCorrect.X = fighter_data->phys.pos.X;
    fighter_data->coll_data.topN_CurrCorrect.Y = fighter_data->phys.pos.Y;
    fighter_data->coll_data.topN_Proj.X = fighter_data->phys.pos.X;
    fighter_data->coll_data.topN_Proj.Y = fighter_data->phys.pos.Y;

    // Update Collision Frame ID
    fighter_data->coll_data.coll_test = *stc_colltest;

    // Adjust JObj position (code copied from 8006c324)
    JOBJ *fighter_jobj = fighter->hsd_object;
    fighter_jobj->trans.X = fighter_data->phys.pos.X;
    fighter_jobj->trans.Y = fighter_data->phys.pos.Y;
    fighter_jobj->trans.Z = fighter_data->phys.pos.Z;
    JOBJ_SetMtxDirtySub(fighter_jobj);

    // Update Static Player Block Coords
    Fighter_SetPosition(fighter_data->ply, fighter_data->flags.ms, &fighter_data->phys.pos);
}
void Fighter_UpdateCamera(GOBJ *fighter)
{
    FighterData *fighter_data = fighter->userdata;

    // Update camerabox pos
    Fighter_UpdateCameraBox(fighter);

    // Update tween
    fighter_data->camera_subject->boundleft_curr = fighter_data->camera_subject->boundleft_proj;
    fighter_data->camera_subject->boundright_curr = fighter_data->camera_subject->boundright_proj;

    // update camera position
    Match_CorrectCamera();

    // reset onscreen bool
    //Fighter_UpdateOnscreenBool(fighter);
    fighter_data->flags.is_offscreen = 0;
}
void RebirthWait_Phys(GOBJ *fighter)
{

    FighterData *fighter_data = fighter->userdata;

    // infinite time
    fighter_data->state_var.state_var1 = 2;
}
int RebirthWait_IASA(GOBJ *fighter)
{

    FighterData *fighter_data = fighter->userdata;
    if (!Fighter_IASACheck_JumpAerial(fighter))
    {
        ftCommonData *ftcommon = *stc_ftcommon;

        // check for lstick movement
        float stick_x = fabs(fighter_data->input.lstick.X);
        float stick_y = fighter_data->input.lstick.Y;
        if (
            (stick_x > 0.2875f && fighter_data->input.timer_lstick_tilt_x < 2)
            || (stick_y < -ftcommon->lstick_rebirthfall && fighter_data->input.timer_lstick_tilt_y < 4)
        ) {
            Fighter_EnterFall(fighter);
            return 1;
        }
    }

    return 0;
}
int Fighter_IsFallInput(FighterData *hmn_data)
{
    float thresh = (*stc_ftcommon)->ledge_drop_thresh; // 0.2875
    float drop_angle = (*stc_ftcommon)->lstick_tilt;  // 0.872665 (50 deg)
    float lx = hmn_data->input.lstick.X;
    float ly = hmn_data->input.lstick.Y;
    float angle = atan2(ly, fabs(lx));
    float cx = hmn_data->input.cstick.X;
    float cy = hmn_data->input.cstick.Y;

    // Technically there are some false positives here that result in a ledgejump.
    // However, this shouldn't affect the tip displays.
    int lstick_drop = (fabs(lx) >= thresh || fabs(ly) >= thresh) &&
        !(angle > drop_angle || (angle > -drop_angle && lx * hmn_data->facing_direction >= 0));
    int cstick_drop = (fabs(cx) >= thresh || fabs(cy) >= thresh);
    return (lstick_drop || cstick_drop) && !Fighter_IsFallBlocked(hmn_data);
}
int Fighter_IsFallBlocked(FighterData *hmn_data)
{
    float thresh = (*stc_ftcommon)->ledge_drop_thresh; // 0.2875
    float lx = hmn_data->input.lstick_prev.X;
    float ly = hmn_data->input.lstick_prev.Y;
    float cx = hmn_data->input.cstick_prev.X;
    float cy = hmn_data->input.cstick_prev.Y;

    return fabs(lx) >= thresh || fabs(ly) >= thresh ||
        fabs(cx) >= thresh || fabs(cy) >= thresh;
}
// Tips Functions
void Tips_Toggle(GOBJ *menu_gobj, int value)
{
    // destroy existing tips when disabling
    if (value == 1)
        event_vars->Tip_Destroy();
}
void Tips_Think(LedgedashData *event_data, FighterData *hmn_data)
{
    // skip if tips turned off
    if (!LdshOptions_Main[OPT_TIPS].val)
        return;

    // check for early fall input in cliffcatch
    if (!event_data->tip.is_input_release && hmn_data->state_id == ASID_CLIFFCATCH && Fighter_IsFallInput(hmn_data))
    {
        event_data->tip.is_input_release = 1;
        event_vars->Tip_Destroy();

        // determine how many frames early
        Figatree *anim = Fighter_GetAnimData(hmn_data, hmn_data->action_id);
        float frame_num = anim->frame_num;
        float frames_early = frame_num - hmn_data->state.frame;
        event_vars->Tip_Display(3 * 60, "Misinput:\nFell %df early.", (int)frames_early + 1);
    }

    // check for early fall input on cliffwait frame 0
    if (!event_data->tip.is_input_release && hmn_data->state_id == ASID_CLIFFWAIT && hmn_data->TM.state_frame == 1 && Fighter_IsFallInput(hmn_data))
    {
        event_data->tip.is_input_release = 1;
        event_vars->Tip_Destroy();
        event_vars->Tip_Display(LSDH_TIPDURATION, "Misinput:\nFell 1f early.");
    }

    if (!event_data->tip.is_input_release && hmn_data->state_id == ASID_CLIFFJUMPQUICK1)
    {
        if (Fighter_IsFallInput(hmn_data))
        {
            event_data->tip.is_input_release = 1;
            event_vars->Tip_Destroy();

            // jumped before fall
            event_vars->Tip_Display(LSDH_TIPDURATION, "Misinput:\nJumped %df early.", hmn_data->TM.state_frame + 1);
        }
        else if (Fighter_IsFallBlocked(hmn_data))
        {
            event_data->tip.is_input_release = 1;
            event_vars->Tip_Destroy();

            // failed to release sticks to neutral
            event_vars->Tip_Display(LSDH_TIPDURATION, "Misinput:\nDid not reset sticks \nto neutral before dropping.", hmn_data->TM.state_frame);
        }
    }

    // check for ledgedash without refreshing
    if (!event_data->tip.refresh_displayed && event_data->action_state.is_finished && event_data->tip.refresh_num == 0)
    {

        event_data->tip.refresh_displayed = 1;

        // increment condition count
        event_data->tip.refresh_cond_num++;

        // after 3 conditions, display tip
        if (event_data->tip.refresh_cond_num >= 3)
        {
            // if tip is displayed, reset cond num
            if (event_vars->Tip_Display(5 * 60, "Warning:\nIt is higly recommended to\nre-grab ledge after \nbeing reset to simulate \na realistic scenario!"))
                event_data->tip.refresh_cond_num = 0;
        }
    }
}

int Update_CheckPause(void)
{
    HSD_Update *update = stc_hsd_update;
    int isChange = 0;

    // menu paused
    if (Ldsh_FrameAdvance.val == 1)
    {
        // check if unpaused
        if (update->pause_kind != PAUSEKIND_SYS)
            isChange = 1;
    }
    // menu unpaused
    else
    {
        // check if paused
        if (update->pause_kind == PAUSEKIND_SYS)
            isChange = 1;
    }

    return isChange;
}
int Update_CheckAdvance(void)
{
    static int timer = 0;

    HSD_Update *update = stc_hsd_update;
    int isAdvance = 0;

    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    int controller = hmn_data->pad_index;

    // get their pad
    HSD_Pad *pad = PadGetMaster(controller);
    HSD_Pad *engine_pad = PadGetEngine(controller);

    // get their advance input
    static int stc_advance_btns[] = {HSD_TRIGGER_L, HSD_TRIGGER_Z, HSD_BUTTON_X, HSD_BUTTON_Y, HSD_TRIGGER_R};
    u32 btn_idx = TM_GetSetting(TM_SETTING_ADVANCE, 0);
    if (btn_idx >= countof(stc_advance_btns))
        btn_idx = 0;
    int advance_btn = stc_advance_btns[btn_idx];

    // check if holding L
    if (Ldsh_FrameAdvance.val == 1 && (pad->held & advance_btn))
    {
        timer++;

        // advance if first press or holding more than 10 frames
        if (timer == 1 || timer > 30)
        {
            isAdvance = 1;

            // remove button input
            pad->down &= ~advance_btn;
            pad->held &= ~advance_btn;
            engine_pad->down &= ~advance_btn;
            engine_pad->held &= ~advance_btn;

            // if using L, remove analog press too
            if (advance_btn == HSD_TRIGGER_L)
            {
                pad->triggerLeft = 0;
                pad->ftriggerLeft = 0;
                engine_pad->triggerLeft = 0;
                engine_pad->ftriggerLeft = 0;
            }
            else if (advance_btn == HSD_TRIGGER_R)
            {
                pad->triggerRight = 0;
                pad->ftriggerRight = 0;
                engine_pad->triggerRight = 0;
                engine_pad->ftriggerRight = 0;
            }
        }
    }
    else
    {
        update->advance = 0;
        timer = 0;
    }

    return isAdvance;
}


static int Ledgedash_AttackDash(FighterData *data) {
    return Ldsh_IsAttackDash(data->atk_kind, data->state_id);
}
static void Ledgedash_UpdateRate(LedgedashData *data) {
    if (!data->hud.total_count) { sprintf(success_rate_text, "-"); return; }
    float percentage = 100.f * data->hud.successful_count / data->hud.total_count;
    sprintf(success_rate_text, "%d/%d (%.1f%%)", data->hud.successful_count, data->hud.total_count, percentage);
}
static void Ledgedash_UpdateAttempt(LedgedashData *data, FighterData *ft) {
    unsigned criterion = LdshOptions_Main[OPT_CRITERION].val;
    int state = ft->state_id;
    int landing = state == ASID_LANDING || state == ASID_LANDINGFALLSPECIAL ||
        (state >= ASID_LANDINGAIRN && state <= ASID_LANDINGAIRLW);
    int grounded = !ft->phys.air_state && (ft->coll_data.envFlags & ECB_GROUND);
    int actionable = state == ASID_LANDING ? ft->TM.state_frame >= ft->attr.normal_landing_lag :
        !landing && (state == ASID_WAIT || (state >= ASID_WALKSLOW && state <= ASID_RUN) ||
            ft->TM.state_prev[0] == ASID_LANDING || ft->TM.state_prev[0] == ASID_LANDINGFALLSPECIAL ||
            ft->TM.state_prev[1] == ASID_LANDING || ft->TM.state_prev[1] == ASID_LANDINGFALLSPECIAL ||
            Ledgedash_AttackDash(ft));
    LdshSample sample = {
        .on_ledge = state == ASID_CLIFFWAIT,
        .released = data->action_state.is_release,
        .airdodge = state == ASID_ESCAPEAIR || data->action_state.is_airdodge,
        .grounded = grounded, .actionable = actionable,
        .galint = ft->hurt.intang_frames.ledge,
        .attack_dash = grounded && !landing && Ledgedash_AttackDash(ft),
        .egg_pop = egg_target.popped,
        .dead = ft->flags.dead,
        .ledge_option = state >= ASID_CLIFFCLIMBSLOW && state <= ASID_CLIFFJUMPQUICK2,
        .frozen = ft->flags.hitlag || ft->flags.freeze,
        .target_ready = egg_target.available,
    };
    int result = LdshAttempt_Step(&data->attempt, &sample, criterion);
    sprintf(criterion_text, "%s%s", LdshCriterionLabels[criterion],
        result == LDSH_PENDING && data->attempt.phase == LDSH_LANDED ? "*" : "");
    if (data->attempt.landing_complete) {
        int galint = data->attempt.galint_at_landing;
        if (galint > 0) sprintf(galint_text, "%df", galint);
        else if (ft->TM.vuln_frames < 25) sprintf(galint_text, "-%df", ft->TM.vuln_frames);
        else sprintf(galint_text, "-");
    }
    if (result == LDSH_PENDING || data->attempt.counted) return;
    data->attempt.counted = 1;
    data->action_state.is_finished = 1;
    data->was_successful = result == LDSH_SUCCESS;
    if (result != LDSH_SETUP_UNAVAILABLE) {
        if (data->hud.total_count < 999999) {
            data->hud.total_count++;
            if (data->was_successful) data->hud.successful_count++;
        }
        Ledgedash_UpdateRate(data);
        if (data->was_successful) SFX_Play(303);
        else SFX_PlayCommon(3);
    } else event_vars->Tip_Display(180, "Egg target unavailable.\nThis setup was not counted.");
    if (data->action_state.is_airdodge)
        sprintf(angle_text, "%.2f", fabs(data->hud.airdodge_angle / M_1DEGREE));
    else sprintf(angle_text, "-");
    ((LdshHitlogData *)data->hitlog_gobj->userdata)->num = 0;
}
static void Ledgedash_ApplyOverlay(LedgedashData *data, GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    int action = LDACT_NONE;
    unsigned frame = data->action_state.timer ? data->action_state.timer - 1 : 30;
    if (frame < countof(data->action_state.action_log)) action = data->action_state.action_log[frame];
    GXColor color = action_colors[action];
    int colored = LdshOptions_Main[OPT_OVERLAYS].val && frame < countof(data->action_state.action_log);
    int protected = LdshOptions_Main[OPT_PROTECTION].val && event_vars->is_protected(fighter);
    if (protected) {
        if (!colored || action == LDACT_NONE) color = action_colors[LDACT_GALINT];
        color.r += (255 - color.r) * 2 / 5;
        color.g += (255 - color.g) * 2 / 5;
        color.b += (255 - color.b) * 2 / 5;
        color.a = 230;
    }
    event_vars->set_local_overlay(fighter, !ft->flags.dead && (colored || protected) ? &color : 0);
}
static void Ledgedash_ChangeLedge(GOBJ *menu, int value) {
    LedgedashData *data = event_vars->event_gobj->userdata;
    data->ledge = value ? 1 : -1;
    Fighter_PlaceOnLedge();
}
static void Ledgedash_ChangeCriterion(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_LEDGEDASH, TM_LEDGE_CRITERION, value);
    LedgedashData *data = event_vars->event_gobj->userdata;
    data->hud.total_count = 0; data->hud.successful_count = 0;
    if (value == LDSH_POP_EGG && !LdshOptions_EggMenu[EGG_ENABLE].val) LdshOptions_EggMenu[EGG_ENABLE].val = 1;
    sprintf(criterion_text, "%s", LdshCriterionLabels[value]);
    Ledgedash_UpdateRate(data);
    Fighter_PlaceOnLedge();
}
static void Ledgedash_ChangeEgg(GOBJ *menu, int value) {
    if (LdshOptions_Main[OPT_CRITERION].val == LDSH_POP_EGG && !LdshOptions_EggMenu[EGG_ENABLE].val)
        LdshOptions_EggMenu[EGG_ENABLE].val = 1;
    Fighter_PlaceOnLedge();
}
static int Ledgedash_ItemLive(GOBJ *object) {
    if (!object) return 0;
    for (GOBJ *item = (*stc_gobj_lookup)[MATCHPLINK_ITEM]; item; item = item->next)
        if (item == object) return ((ItemData *)item->userdata)->kind == ITEM_EGG;
    return 0;
}
static void Ledgedash_EggCleanup(void) {
    if (Ledgedash_ItemLive(egg_target.object)) {
        ItemData *ip = egg_target.object->userdata;
        if (ip->it_func && ip->it_func->OnTakeDamage == Ledgedash_EggDamage)
            ip->it_func->OnTakeDamage = egg_target.original_damage;
        Item_Destroy(egg_target.object);
    }
    if (Ledgedash_ItemLive(egg_target.retiring)) Item_Destroy(egg_target.retiring);
    memset(&egg_target, 0, sizeof(egg_target));
    sprintf(egg_text, "Off");
}
static void Ledgedash_CleanupScene(void *unused) { Ledgedash_EggCleanup(); }
static int Ledgedash_FindEggSurface(LedgedashData *data, int platform, int distance, Vec3 *position) {
    CollDataStage *stage = *stc_colldata;
    CollLine *lines = *stc_collline;
    CollVert *verts = *stc_collvert;
    if (!stage || !lines || !verts) return -1;
    float desired = data->ledge_pos.X + data->ledge_dir * distance;
    float best = 100000;
    int found = -1;
    for (int i = 0; i < stage->line_num; ++i) {
        CollLineDesc *line = lines[i].desc;
        if (!line || !line->is_floor || line->disabled || !GrColl_CheckIfLineEnabled(i) ||
            line->vert_prev < 0 || line->vert_next < 0 ||
            line->vert_prev >= stage->vert_num || line->vert_next >= stage->vert_num) continue;
        Vec2 a = verts[line->vert_prev].pos_curr, b = verts[line->vert_next].pos_curr;
        LdshSurface surface = {a.X, a.Y, b.X, b.Y, line->is_drop, 1};
        float x, y;
        if (!LdshSurface_Target(&surface, desired, data->ledge_pos.Y, platform, &x, &y)) continue;
        if ((x - data->ledge_pos.X) * data->ledge_dir < 5 || fabs(x - desired) > 60) continue;
        float score = fabs(x - desired) + fabs(y - data->ledge_pos.Y) * 0.25f;
        if (score < best) { best = score; found = i; *position = (Vec3){x, y, 0}; }
    }
    return found;
}
static void Ledgedash_EggReset(LedgedashData *data, int reroll) {
    Ledgedash_EggCleanup();
    if (!LdshOptions_EggMenu[EGG_ENABLE].val) return;
    if (reroll || data->egg_placement.distance < 5) {
        int options[] = {LdshOptions_EggMenu[EGG_TARGET].val, LdshOptions_EggMenu[EGG_DISTANCE].val,
            LdshOptions_EggMenu[EGG_RANDOM_DISTANCE].val, LdshOptions_EggMenu[EGG_MIN_DISTANCE].val,
            LdshOptions_EggMenu[EGG_MAX_DISTANCE].val};
        int span = options[3] > options[4] ? options[3] - options[4] : options[4] - options[3];
        unsigned target_roll = options[0] == 2 ? HSD_Randi(2) : 0;
        unsigned distance_roll = options[2] ? HSD_Randi(span + 1) : 0;
        LdshEgg_Choose(&data->egg_placement, options, 1, target_roll, distance_roll);
    }
    int mode = data->egg_placement.mode, distance = data->egg_placement.distance;
    Vec3 pos;
    int line = Ledgedash_FindEggSurface(data, mode == 1, distance, &pos);
    int fallback = 0;
    if (line < 0 && mode == 1) { line = Ledgedash_FindEggSurface(data, 0, distance, &pos); fallback = 1; }
    if (line < 0) { sprintf(egg_text, "Unavailable"); return; }
    SpawnItem spawn = {.it_kind = ITEM_EGG, .pos = pos, .pos2 = pos, .vel = {0,0,0}};
    GOBJ *object = Item_CreateItem2(&spawn);
    if (!object) { sprintf(egg_text, "Unavailable"); return; }
    egg_target.object = object; egg_target.line = line; egg_target.available = 1;
    ItemData *ip = object->userdata;
    ip->pos = pos; ip->self_vel = (Vec3){0,0,0};
    Coll_CopyPosToECBs(&ip->coll_data, &pos);
    ip->coll_data.ground_index = line;
    ip->coll_data.envFlags = ip->coll_data.envFlags_prev = ECB_GROUND;
    GrColl_GetLineSlope(line, &ip->coll_data.ground_slope);
    Item_SetGrounded(ip);
    Egg_EnterRest(object); // Ready on the selected floor; no Eggsercize-style falling launch.
    ip->can_hold = 0; ip->can_nudge = 0;
    egg_target.original_damage = ip->it_func->OnTakeDamage;
    ip->it_func->OnTakeDamage = Ledgedash_EggDamage;
    CollLineDesc *desc = (*stc_collline)[line].desc;
    Vec2 left = (*stc_collvert)[desc->vert_prev].pos_curr;
    Vec2 right = (*stc_collvert)[desc->vert_next].pos_curr;
    egg_target.fraction = (pos.X - left.X) / (right.X - left.X);
    Item_UpdatePositionCollision(object);
    sprintf(egg_text, "%s", fallback ? "Ground (fallback)" : mode == 1 ? "Platform" : "Ground");
}
static int Ledgedash_EggDamage(GOBJ *object) {
    if (object != egg_target.object || !Ledgedash_ItemLive(object)) return 0;
    ItemData *ip = object->userdata;
    GOBJ *fighter = Fighter_GetGObj(0);
    FighterData *ft = fighter->userdata;
    /* Only the player's own current target hit can award the protected pop. */
    if (ip->dmg.source_ply != (u8)ft->ply) return 0;
    egg_target.accumulated += ip->dmg.recent;
    if (egg_target.accumulated < LdshOptions_EggMenu[EGG_DAMAGE].val) return 0;
    LedgedashData *data = event_vars->event_gobj->userdata;
    int state = ft->state_id;
    int recovered_now = !ft->phys.air_state &&
        state != ASID_LANDING && state != ASID_LANDINGFALLSPECIAL &&
        !(state >= ASID_LANDINGAIRN && state <= ASID_LANDINGAIRLW) &&
        (data->attempt.landing_complete || ft->TM.state_prev[0] == ASID_LANDING ||
         ft->TM.state_prev[0] == ASID_LANDINGFALLSPECIAL || state == ASID_WAIT);
    egg_target.popped = (data->attempt.landing_complete || recovered_now) && data->action_state.is_release && !ft->phys.air_state &&
        ft->hurt.intang_frames.ledge > 0 && data->attempt.phase != LDSH_RESOLVED;
    ip->it_func->OnTakeDamage = egg_target.original_damage;
    Effect_SpawnSync(1232, object, ip->pos);
    Item_PlayOnDestroySFXAgain(ip, 244, 127, 64);
    Egg_Destroy(object);
    egg_target.retiring = object; egg_target.object = 0;
    sprintf(egg_text, "Popped");
    return 0;
}
static void Ledgedash_EggThink(LedgedashData *data) {
    if (!egg_target.object) return;
    if (!Ledgedash_ItemLive(egg_target.object)) {
        egg_target.object = 0; egg_target.available = 0;
        sprintf(egg_text, "Unavailable"); return;
    }
    if (!GrColl_CheckIfLineEnabled(egg_target.line)) { Ledgedash_EggReset(data, 0); return; }
    CollLineDesc *desc = (*stc_collline)[egg_target.line].desc;
    Vec2 a = (*stc_collvert)[desc->vert_prev].pos_curr, b = (*stc_collvert)[desc->vert_next].pos_curr;
    ItemData *ip = egg_target.object->userdata;
    ip->pos = (Vec3){a.X + (b.X - a.X) * egg_target.fraction,
                    a.Y + (b.Y - a.Y) * egg_target.fraction, 0};
    ip->self_vel = (Vec3){0,0,0}; ip->can_hold = 0; ip->can_nudge = 0;
    ip->coll_data.ground_index = egg_target.line;
    Coll_CopyPosToECBs(&ip->coll_data, &ip->pos);
    ip->it_func->OnTakeDamage = Ledgedash_EggDamage;
    Item_UpdatePositionCollision(egg_target.object);
}

// Initial Menu
EventMenu *Event_Menu = &LdshMenu_Main;
