/* Training Lab presentation only. Every existing setting references its
 * original option; array/value indices used by recordings and saves are intact.
 * Included after lab.h and recovery.c in the Lab translation unit. */
#define LR(a, i, g) { .option = &(a)[i], .group = g }
#define LREF(o, g) { .option = &(o), .group = g }
#define LMENU(id, title, description) \
    static EventMenu id = { .name = title, .rows = id##_rows, \
        .option_num = countof(id##_rows), .purpose = description }
#define LLINK(id, title, dest, description) \
    static EventOption id = { .kind = OPTKIND_MENU, .name = title, \
        .menu = &dest, .desc = { description } }

static EventMenu LabUI_DI, LabUI_Tech, LabUI_TechChances, LabUI_GetupChances;
static EventMenu LabUI_TechFeedback, LabUI_Counter, LabUI_Shield, LabUI_Position;
static EventMenu LabUI_Playback, LabUI_Files, LabUI_Chances;
static EventMenu LabUI_Collision, LabUI_Fighters, LabUI_Tools;
static EventMenu LabUI_Movement, LabUI_Combat, LabUI_Timing, LabUI_GlobalVisuals;

LLINK(LabLink_DI, "DI & Survival", LabUI_DI, "Choose CPU escape inputs during hits and grabs.");
LLINK(LabLink_TechChances, "Tech Chances", LabUI_TechChances, "Weight the random tech outcomes.");
LLINK(LabLink_GetupChances, "Getup Chances", LabUI_GetupChances, "Weight waiting and random getup outcomes.");
LLINK(LabLink_TechFeedback, "Tech Feedback & Traps", LabUI_TechFeedback, "Choose tech cues and practice trap conditions.");
LLINK(LabLink_Counter, "Counter Actions", LabUI_Counter, "Choose the CPU response after hits or shielding.");
LLINK(LabLink_Shield, "Shield & Protection", LabUI_Shield, "Choose local shield policy and CPU protection.");
LLINK(LabLink_Position, "Position & Control", LabUI_Position, "Move, freeze or manually control the CPU.");
LLINK(LabLink_Playback, "Playback Rules", LabUI_Playback, "Loop repeats inputs; Auto Restore reloads the saved situation. Also choose CPU handoff, takeover and mirroring.");
LLINK(LabLink_Files, "Positions & Files", LabUI_Files, "Re-Save keeps complete clips. Prune removes the played beginning. Delete clears all clips; Export writes a card file.");
LLINK(LabLink_Chances, "Playback Chances", LabUI_Chances, "Weight the clips selected by Random playback and add random extra percent on each restore.");
LLINK(LabLink_Collision, "Collision & Bounds", LabUI_Collision, "Inspect fighter, item and stage collision.");
LLINK(LabLink_Fighters, "Fighter Displays", LabUI_Fighters, "Choose human or CPU diagnostics and overlays.");
LLINK(LabLink_Tools, "OSD Tools", LabUI_Tools, "Create feedback from the human action states.");
LLINK(LabLink_Movement, "Movement & Landing OSDs", LabUI_Movement, "Choose movement and landing OSD colors.");
LLINK(LabLink_Combat, "Combat & Defense OSDs", LabUI_Combat, "Choose combat, grab and defense OSD colors.");
LLINK(LabLink_Timing, "Action Timing OSDs", LabUI_Timing, "Choose action and interrupt OSD colors.");
LLINK(LabLink_GlobalVisuals, "Global Visuals & Shields", LabUI_GlobalVisuals, "Shared trails, fighter cues and shield refill.");

static EventMenuRow LabUI_Session_rows[] = {
    LR(LabOptions_General, OPTGEN_FRAME, 1), LR(LabOptions_General, OPTGEN_SPEED, 1),
    LR(LabOptions_General, OPTGEN_HMNPCNT, 2), LR(LabOptions_General, OPTGEN_HMNPCNTLOCK, 2),
    LR(LabOptions_General, OPTGEN_STALE, 2),
};
LMENU(LabUI_Session, "Session", "Set practice speed and the human starting damage.");
static EventMenuRow LabUI_CPU_rows[] = {
    LR(LabOptions_CPU, OPTCPU_PCNT, 2), LR(LabOptions_CPU, OPTCPU_LOCKPCNT, 2), LR(LabOptions_CPU, OPTCPU_BEHAVE, 2),
    LREF(LabLink_DI, 1), LR(LabOptions_CPU, OPTCPU_TECHOPTIONS, 3), LREF(LabLink_Counter, 0),
    LREF(LabLink_Shield, 4), LREF(LabLink_Position, 0), LR(LabOptions_CPU, OPTCPU_RECOVERY, 0),
};
LMENU(LabUI_CPU, "CPU", "Configure the practice opponent.");
static EventMenuRow LabUI_DI_rows[] = {
    LR(LabOptions_CPU, OPTCPU_TDI, 1), LR(LabOptions_CPU, OPTCPU_CUSTOMTDI, 1),
    LR(LabOptions_CPU, OPTCPU_SDINUM, 1), LR(LabOptions_CPU, OPTCPU_SDIDIR, 1), LR(LabOptions_CPU, OPTCPU_ASDI, 1),
    LR(LabOptions_CPU, OPTCPU_MASH, 4), LR(LabOptions_CPU, OPTCPU_GRABRELEASE, 4),
};
LMENU(LabUI_DI, "DI & Survival", "Choose CPU escape inputs during hits and grabs.");
static EventMenuRow LabUI_Tech_rows[] = {
    LR(LabOptions_Tech, OPTTECH_TECH, 0), LR(LabOptions_Tech, OPTTECH_GETUP, 0),
    LREF(LabLink_TechChances, 3), LREF(LabLink_GetupChances, 3), LREF(LabLink_TechFeedback, 4),
};
LMENU(LabUI_Tech, "Tech & Getup", "Choose recovery modes, probabilities and traps.");
static EventMenuRow LabUI_TechChances_rows[] = {
    LR(LabOptions_Tech, OPTTECH_TECHINPLACECHANCE, 3), LR(LabOptions_Tech, OPTTECH_TECHAWAYCHANCE, 3),
    LR(LabOptions_Tech, OPTTECH_TECHTOWARDCHANCE, 3), LR(LabOptions_Tech, OPTTECH_MISSTECHCHANCE, 3),
};
LMENU(LabUI_TechChances, "Tech Chances", "Probabilities apply when Tech Option is Random.");
static EventMenuRow LabUI_GetupChances_rows[] = {
    LR(LabOptions_Tech, OPTTECH_GETUPWAITCHANCE, 0), LR(LabOptions_Tech, OPTTECH_GETUPSTANDCHANCE, 3),
    LR(LabOptions_Tech, OPTTECH_GETUPAWAYCHANCE, 3), LR(LabOptions_Tech, OPTTECH_GETUPTOWARDCHANCE, 3),
    LR(LabOptions_Tech, OPTTECH_GETUPATTACKCHANCE, 3),
};
LMENU(LabUI_GetupChances, "Getup Chances", "Stand/roll/attack chances apply to Random getup.");
static EventMenuRow LabUI_TechFeedback_rows[] = {
    LR(LabOptions_Tech, OPTTECH_INVISIBLE, 3), LR(LabOptions_Tech, OPTTECH_INVISIBLE_DELAY, 3), LR(LabOptions_Tech, OPTTECH_SOUND, 3),
    LR(LabOptions_Tech, OPTTECH_TRAP, 4), LR(LabOptions_Tech, OPTTECH_LOCKOUT, 4),
};
LMENU(LabUI_TechFeedback, "Tech Feedback & Traps", "Choose tech cues and practice trap conditions.");
static EventMenuRow LabUI_Counter_rows[] = {
    LR(LabOptions_CPU, OPTCPU_CTRGRND, 2), LR(LabOptions_CPU, OPTCPU_CTRAIR, 1), LR(LabOptions_CPU, OPTCPU_CTRSHIELD, 4),
    LR(LabOptions_CPU, OPTCPU_CTRFRAMES, 0), LR(LabOptions_CPU, OPTCPU_CTRADV, 0),
};
LMENU(LabUI_Counter, "Counter Actions", "Choose default responses or customize each hit.");
static EventMenuRow LabUI_Shield_rows[] = {
    LR(LabOptions_CPU, OPTCPU_SHIELD, 4), LR(LabOptions_CPU, OPTCPU_SHIELDHEALTH, 4), LR(LabOptions_CPU, OPTCPU_SHIELDDIR, 4),
    LR(LabOptions_CPU, OPTCPU_INTANG, 3),
};
LMENU(LabUI_Shield, "Shield & Protection", "Local shield policy; Global Infinite Shields overrides it.");
static EventMenuRow LabUI_Position_rows[] = {
    LR(LabOptions_CPU, OPTCPU_SET_POS, 2), LR(LabOptions_CPU, OPTCPU_CTRL_BY, 2), LR(LabOptions_CPU, OPTCPU_FREEZE, 2),
};
LMENU(LabUI_Position, "Position & Control", "Move, freeze or manually control the CPU.");

static EventMenuRow LabUI_Record_rows[] = {
    LR(LabOptions_Record, OPTREC_SAVE_LOAD, 2), LR(LabOptions_Record, OPTREC_HMNMODE, 1), LR(LabOptions_Record, OPTREC_HMNSLOT, 1),
    LR(LabOptions_Record, OPTREC_CPUMODE, 3), LR(LabOptions_Record, OPTREC_CPUSLOT, 3),
    LREF(LabLink_Playback, 0), LREF(LabLink_Files, 0), LR(LabOptions_Record, OPTREC_SLOTMANAGEMENT, 0), LREF(LabLink_Chances, 0),
};
LMENU(LabUI_Record, "Recording", "1. Save Positions. 2. Choose human/CPU slot and Record. 3. Unpause and perform inputs. 4. Select Playback to replay.");
static EventMenuRow LabUI_Playback_rows[] = {
    LR(LabOptions_Record, OPTREC_MIRRORED_PLAYBACK, 0), LR(LabOptions_Record, OPTREC_PLAYBACK_COUNTER, 3),
    LR(LabOptions_Record, OPTREC_LOOP, 0), LR(LabOptions_Record, OPTREC_AUTORESTORE, 2),
    LR(LabOptions_Record, OPTREC_STARTPAUSED, 2), LR(LabOptions_Record, OPTREC_TAKEOVER, 0),
};
LMENU(LabUI_Playback, "Playback Rules", "Loop repeats inputs without resetting fighters. Auto Restore reloads the saved match state. Choose takeover and CPU handoff here too.");
static EventMenuRow LabUI_Files_rows[] = {
    LR(LabOptions_Record, OPTREC_RESAVE, 2), LR(LabOptions_Record, OPTREC_PRUNE, 2),
    LR(LabOptions_Record, OPTREC_DELETE, 4), LR(LabOptions_Record, OPTREC_EXPORT, 0),
};
LMENU(LabUI_Files, "Positions & Files", "Change the shared starting situation, trim or delete clips, or save the whole recording setup to a memory card.");
static EventMenuRow LabUI_Chances_rows[] = {
    LR(LabOptions_Record, OPTREC_HMNCHANCE, 1), LR(LabOptions_Record, OPTREC_CPUCHANCE, 3),
};
LMENU(LabUI_Chances, "Playback Chances", "Choose probabilities for Random playback. Only recorded slots participate; Random Percent adds extra damage when restoring.");

static EventMenuRow LabUI_Visual_rows[] = {
    LR(LabOptions_General, OPTGEN_MODEL, 3), LREF(LabLink_Collision, 0), LR(LabOptions_General, OPTGEN_CAM, 3),
    LR(LabOptions_General, OPTGEN_HUD, 3), LR(LabOptions_General, OPTGEN_DI, 1), LR(LabOptions_General, OPTGEN_INPUT, 3),
    LREF(LabLink_Fighters, 0), LR(LabOptions_General, OPTLAB_HITBOXTRAILS, 1), LREF(LabLink_Tools, 0),
};
LMENU(LabUI_Visual, "Visual Feedback", "Choose displays, fighter diagnostics and local trails.");
static EventMenuRow LabUI_Collision_rows[] = {
    LR(LabOptions_General, OPTGEN_HIT, 3), LR(LabOptions_General, OPTGEN_ITEMGRAB, 3), LR(LabOptions_General, OPTGEN_COLL, 3),
};
LMENU(LabUI_Collision, "Collision & Bounds", "Inspect fighter, item and stage collision.");
static EventMenuRow LabUI_Fighters_rows[] = {
    LR(LabOptions_Main, OPTLAB_INFODISP_HMN, 1), LR(LabOptions_Main, OPTLAB_INFODISP_CPU, 3),
    LR(LabOptions_General, OPTGEN_OVERLAYS_HMN, 1), LR(LabOptions_General, OPTGEN_OVERLAYS_CPU, 3),
};
LMENU(LabUI_Fighters, "Fighter Displays", "Choose human or CPU diagnostics and overlays.");
static EventMenuRow LabUI_Tools_rows[] = {
    LR(LabOptions_General, OPTGEN_CUSTOM_OSD, 0), LR(LabOptions_General, OPTLAB_ACTIONLOG, 0),
};
LMENU(LabUI_Tools, "OSD Tools", "Create feedback from the human action states.");

static EventMenuRow LabUI_Global_rows[] = {
    LR(LabOptions_OSDs, TM_SETTINGS_OSDS + 1, 0), LR(LabOptions_OSDs, TM_SETTINGS_OSDS, 0),
    LREF(LabLink_Movement, 1), LREF(LabLink_Combat, 4), LREF(LabLink_Timing, 3),
    LR(LabOptions_OSDs, TM_SETTINGS_OSDS + 10, 0), LREF(LabLink_GlobalVisuals, 0),
};
LMENU(LabUI_Global, "Global Settings", "Saved shared preferences apply across events.");
static EventMenuRow LabUI_Movement_rows[] = {
    LR(LabOptions_OSDs, 0, 1), LR(LabOptions_OSDs, 1, 1), LR(LabOptions_OSDs, 3, 1),
    LR(LabOptions_OSDs, 12, 1), LR(LabOptions_OSDs, 13, 1), LR(LabOptions_OSDs, 17, 1),
};
LMENU(LabUI_Movement, "Movement & Landing OSDs", "Global OSD colors for movement and landing.");
static EventMenuRow LabUI_Combat_rows[] = {
    LR(LabOptions_OSDs, 2, 4), LR(LabOptions_OSDs, 5, 4), LR(LabOptions_OSDs, 6, 4), LR(LabOptions_OSDs, 9, 4),
    LR(LabOptions_OSDs, 14, 4), LR(LabOptions_OSDs, 15, 4), LR(LabOptions_OSDs, 16, 4), LR(LabOptions_OSDs, 18, 4),
};
LMENU(LabUI_Combat, "Combat & Defense OSDs", "Global combat, grab and defense OSD colors.");
static EventMenuRow LabUI_Timing_rows[] = {
    LR(LabOptions_OSDs, 10, 3), LR(LabOptions_OSDs, 11, 3), LR(LabOptions_OSDs, 7, 3), LR(LabOptions_OSDs, 8, 3), LR(LabOptions_OSDs, 4, 3),
};
LMENU(LabUI_Timing, "Action Timing OSDs", "Global action and interrupt OSD colors.");
static EventMenuRow LabUI_GlobalVisuals_rows[] = {
    LR(LabOptions_OSDs, TM_SETTINGS_OSDS + 3, 1), LR(LabOptions_OSDs, TM_SETTINGS_OSDS + 4, 1),
    LR(LabOptions_OSDs, TM_SETTINGS_OSDS + 5, 3), LR(LabOptions_OSDs, TM_SETTINGS_OSDS + 6, 3), LR(LabOptions_OSDs, TM_SETTINGS_OSDS + 7, 3),
    LR(LabOptions_OSDs, TM_SETTINGS_OSDS + 9, 3), LR(LabOptions_OSDs, TM_SETTINGS_OSDS + 8, 4),
};
LMENU(LabUI_GlobalVisuals, "Global Visuals & Shields", "Shared trails, fighter cues and full shield refill.");
static EventMenuRow LabUI_Stage_rows[] = {
    LR(LabOptions_Main, OPTLAB_STAGE, 0), LR(LabOptions_Main, OPTLAB_CHAR_RNG, 0),
};
LMENU(LabUI_Stage, "Stage & RNG", "Controls depend on the current stage and fighters.");

/* Editor pages are peers, not deeper menus; all rows keep their option identity. */
#define INFO_PAGES(prefix, array, group) \
    static EventMenuRow prefix##_setup[] = { LR(array, OPTINF_PRESET, group), LR(array, OPTINF_SIZE, group) }; \
    static EventMenuRow prefix##_rows[] = { LR(array, OPTINF_ROW1, group), LR(array, OPTINF_ROW2, group), LR(array, OPTINF_ROW3, group), LR(array, OPTINF_ROW4, group), \
        LR(array, OPTINF_ROW5, group), LR(array, OPTINF_ROW6, group), LR(array, OPTINF_ROW7, group), LR(array, OPTINF_ROW8, group) }; \
    static EventMenuPage prefix[] = { {"Setup", countof(prefix##_setup), prefix##_setup}, {"Rows", countof(prefix##_rows), prefix##_rows} }
INFO_PAGES(LabPages_InfoHMN, LabOptions_InfoDisplayHMN, 1);
INFO_PAGES(LabPages_InfoCPU, LabOptions_InfoDisplayCPU, 3);
#define OVERLAY_PAGES(prefix, array) \
    static EventMenuRow prefix##_movement[] = { LR(array, OVERLAY_CROUCH, 1), LR(array, OVERLAY_WAIT, 1), LR(array, OVERLAY_WALK, 1), LR(array, OVERLAY_DASH, 1), \
        LR(array, OVERLAY_RUN, 1), LR(array, OVERLAY_JUMPS_USED, 1), LR(array, OVERLAY_FULLHOP, 1), LR(array, OVERLAY_SHORTHOP, 1) }; \
    static EventMenuRow prefix##_timing[] = { LR(array, OVERLAY_ACTIONABLE, 3), LR(array, OVERLAY_LEDGE_ACTIONABLE, 3), LR(array, OVERLAY_MISSED_LCANCEL, 3), \
        LR(array, OVERLAY_CAN_FASTFALL, 3), LR(array, OVERLAY_AUTOCANCEL, 3), LR(array, OVERLAY_IASA, 3) }; \
    static EventMenuRow prefix##_combat[] = { LR(array, OVERLAY_HITSTUN, 4), LR(array, OVERLAY_INVINCIBLE, 4), LR(array, OVERLAY_SHIELD_STUN, 4) }; \
    static EventMenuPage prefix[] = { {"Movement", countof(prefix##_movement), prefix##_movement}, {"Timing", countof(prefix##_timing), prefix##_timing}, {"Combat", countof(prefix##_combat), prefix##_combat} }
OVERLAY_PAGES(LabPages_OverlayHMN, LabOptions_OverlaysHMN);
OVERLAY_PAGES(LabPages_OverlayCPU, LabOptions_OverlaysCPU);
static EventMenuRow LabInputs_Analog[] = {
    LR(LabOptions_AlterInputs, OPTINPUT_FRAME, 0), LR(LabOptions_AlterInputs, OPTINPUT_LSTICK_X, 1), LR(LabOptions_AlterInputs, OPTINPUT_LSTICK_Y, 1),
    LR(LabOptions_AlterInputs, OPTINPUT_CSTICK_X, 1), LR(LabOptions_AlterInputs, OPTINPUT_CSTICK_Y, 1), LR(LabOptions_AlterInputs, OPTINPUT_TRIGGER, 1),
};
static EventMenuRow LabInputs_Buttons[] = {
    LR(LabOptions_AlterInputs, OPTINPUT_FRAME, 0), LR(LabOptions_AlterInputs, OPTINPUT_A, 3), LR(LabOptions_AlterInputs, OPTINPUT_B, 3), LR(LabOptions_AlterInputs, OPTINPUT_X, 3),
    LR(LabOptions_AlterInputs, OPTINPUT_Y, 3), LR(LabOptions_AlterInputs, OPTINPUT_Z, 3), LR(LabOptions_AlterInputs, OPTINPUT_L, 3), LR(LabOptions_AlterInputs, OPTINPUT_R, 3),
};
static EventMenuPage LabPages_Inputs[] = {
    {"Analog", countof(LabInputs_Analog), LabInputs_Analog}, {"Buttons", countof(LabInputs_Buttons), LabInputs_Buttons},
};
static EventOption LabResume = { .kind = OPTKIND_RESUME, .name = "Resume Practice", .desc = {"Close the pause menu and resume practice."} };
static EventMenuRow LabUI_Exit_rows[] = { LR(LabOptions_Main, OPTLAB_EXIT, 4), LREF(LabResume, 0) };
LMENU(LabUI_Exit, "Exit", "Return to Event Select or resume practice.");

static EventMenuTab LabMenuTabs[8] = {
    {"Session", &LabUI_Session}, {"CPU", &LabUI_CPU}, {"Recording", &LabUI_Record}, {"Visuals", &LabUI_Visual},
    {"Global", &LabUI_Global}, {"Stage/RNG", &LabUI_Stage}, {"Controls", &LabMenu_Controls}, {"Exit", &LabUI_Exit},
};

static void Lab_InitMenuUI(void)
{
    /* Canonical navigation options are also used by hold-Y shortcuts. */
    LabOptions_Main[OPTLAB_CPU_OPTIONS].menu = &LabUI_CPU;
    LabOptions_Main[OPTLAB_RECORD_OPTIONS].menu = &LabUI_Record;
    LabOptions_CPU[OPTCPU_TECHOPTIONS].menu = &LabUI_Tech;
    LabOptions_CPU[OPTCPU_TECHOPTIONS].name = "Tech & Getup";
    LabOptions_General[OPTGEN_OSDS].menu = &LabUI_Global;
    LabMenu_InfoDisplayHMN.pages = LabPages_InfoHMN; LabMenu_InfoDisplayHMN.page_num = countof(LabPages_InfoHMN);
    LabMenu_InfoDisplayCPU.pages = LabPages_InfoCPU; LabMenu_InfoDisplayCPU.page_num = countof(LabPages_InfoCPU);
    LabMenu_OverlaysHMN.pages = LabPages_OverlayHMN; LabMenu_OverlaysHMN.page_num = countof(LabPages_OverlayHMN);
    LabMenu_OverlaysCPU.pages = LabPages_OverlayCPU; LabMenu_OverlaysCPU.page_num = countof(LabPages_OverlayCPU);
    LabMenu_AlterInputs.pages = LabPages_Inputs; LabMenu_AlterInputs.page_num = countof(LabPages_Inputs);
    LabMenu_Controls.purpose = "Assign frame stepping and unpaused D-pad shortcuts.";
    LabMenu_InfoDisplayHMN.purpose = "Choose human display preset/size or customize eight rows.";
    LabMenu_InfoDisplayCPU.purpose = "Choose CPU display preset/size or customize eight rows.";
    LabMenu_OverlaysHMN.purpose = "Saved human overlays grouped by movement, timing and combat.";
    LabMenu_OverlaysCPU.purpose = "Saved CPU overlays grouped by movement, timing and combat.";
    LabMenu_AlterInputs.purpose = "Edit analog inputs or buttons on the same recorded frame.";
    for (int i = 0; i < REC_SLOTS; ++i) {
        LabOptions_SlotChancesHMN[i].desc[1] = LabOptions_SlotChancesCPU[i].desc[1] = "Used when this actor's Record Slot is Random.";
        LabOptions_SlotChancesHMN[i].desc[2] = LabOptions_SlotChancesCPU[i].desc[2] = "Only recorded slots participate; chances total 100%.";
        LabOptions_SlotChancesHMN[i].desc[3] = LabOptions_SlotChancesCPU[i].desc[3] = "Changing one chance rebalances the other slots.";
    }
    LabOptions_SlotChancesHMN[OPTSLOTCHANCE_PERCENT].desc[0] = LabOptions_SlotChancesCPU[OPTSLOTCHANCE_PERCENT].desc[0] = "Add 0 through this much extra damage on each restore.";
    LabOptions_SlotChancesHMN[OPTSLOTCHANCE_PERCENT].desc[1] = LabOptions_SlotChancesCPU[OPTSLOTCHANCE_PERCENT].desc[1] = "Added to the saved percent, capped at 999%.";
    LabOptions_SlotChancesHMN[OPTSLOTCHANCE_PERCENT].desc[2] = LabOptions_SlotChancesCPU[OPTSLOTCHANCE_PERCENT].desc[2] = "Independent of choosing a Random playback slot.";
}

#undef LR
#undef LREF
#undef LMENU
#undef LLINK
#undef INFO_PAGES
#undef OVERLAY_PAGES
