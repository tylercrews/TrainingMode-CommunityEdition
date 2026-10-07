#ifndef _EGGS_H_
#define _EGGS_H_

#include "../MexTK/mex.h"
#include "events.h"

enum options_main
{
    OPT_RETRY,
    OPT_FREEPRACTICE,
    OPT_DAMAGETHRESHOLD,
    OPT_SCALE,
    OPT_VELOCITY,
    OPT_COLLISION,
    OPT_HITBOXTRAILS,
    OPT_DEFAULTS,
    OPT_EXIT,

    OPT_COUNT
};

void StartFreePractice(GOBJ *gobj);
GOBJ *Egg_Spawn(void);
void Exit(GOBJ *menu);
void Retry(GOBJ *menu);
void Egg_OnChangeSize(GOBJ *menu, int value);
void ChangeHitDisplay(GOBJ *menu_gobj, int value);
void Event_Init(GOBJ *gobj);
float RandomRange(float low, float high);
int Egg_OnTakeDamage(GOBJ *gobj);
void Event_Think(GOBJ *event);
void Eggs_ChangeHitboxTrails(GOBJ *menu_gobj, int value);
void Eggs_ChangeSavedSetting(GOBJ *menu_gobj, int value);
void Eggs_ResetSettings(GOBJ *menu);

static GXColor text_gold = {255, 211, 0, 255};
static GXColor text_white = {255, 255, 255, 255};

static float EggOptions_Size[] = {1.0f, .5f, 2.0f};
static const char *EggOptions_SizeText[] = {"Normal", "Small", "Large"};

enum options_hitbox_trails
{
    OPT_HITBOXTRAILS_ENABLED,
    OPT_HITBOXTRAILS_DECAY,
    OPT_HITBOXTRAILS_INFO,

    OPT_HITBOXTRAILS_COUNT
};

static const char *HitboxTrailDecayText[] = { TM_TRAIL_DECAY_LABELS };

static EventOption Options_HitboxTrails[OPT_HITBOXTRAILS_COUNT] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Enable (local)",
        .desc = {"Enable trails when both global profiles are Off."},
        .OnChange = Eggs_ChangeHitboxTrails,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = countof(HitboxTrailDecayText),
        .name = "Decay",
        .desc = {"How quickly the hitbox will fade away."},
        .values = HitboxTrailDecayText,
        .OnChange = Eggs_ChangeHitboxTrails,
    },
    {
        .kind = OPTKIND_INFO,
        .name = "Global: Off",
        .desc = {"Global Very Fast/Instant override local controls.", "Both On draws the Very Fast union once.", "Change global trails in the L-button OSD menu."},
    },
};

static EventMenu Menu_HitboxTrails = {
    .name = "Hitbox Trails",
    .option_num = countof(Options_HitboxTrails),
    .options = Options_HitboxTrails,
};

static EventOption Options_Main[OPT_COUNT] = {
    {
        .kind = OPTKIND_FUNC,
        .name = "Retry",
        .desc = { "Retry this event. Pressing Z while paused also does this." },
        .OnSelect = Retry
    },
    {
        .kind = OPTKIND_FUNC,
        .name = "Enable Free Practice",
        .desc = { "Start Free Practice mode." },
        .OnSelect = StartFreePractice
    },
    {
        .kind = OPTKIND_INT,
        .name = "Egg Damage Threshold",
        .desc = { "Adjust the minimum damage needed to break an egg." },
        .val = TM_EGGS_DEFAULT_DAMAGE,
        .format = "%d",
        .value_min = 0,
        .value_num = 200,
        .disable = 1,
        .OnChange = Eggs_ChangeSavedSetting,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = countof(EggOptions_SizeText),
        .name = "Egg Scale",
        .desc = { "Adjust egg size." },
        .values = EggOptions_SizeText,
        .disable = 1,
        .OnChange = Egg_OnChangeSize
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Egg Spawn Velocity",
        .desc = { "Toggle whether eggs spawn with vertical velocity." },
        .val = TM_EGGS_DEFAULT_VELOCITY,
        .disable = 1,
        .OnChange = Eggs_ChangeSavedSetting,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fighter Collision",
        .desc = {"Toggle hitbox and hurtbox visualization.",
                 "Hurtboxes: yellow=hurt, purple=ungrabbable, blue=shield.",
                 "Hitboxes: (by priority) red, green, blue, purple."},
        .disable = 1,
        .OnChange = ChangeHitDisplay,
    },
    {
        .kind = OPTKIND_MENU,
        .menu = &Menu_HitboxTrails,
        .name = "Hitbox Trails",
        .desc = {"Create a trail of your hitboxes to visualize spacing."},
    },
    {
        .kind = OPTKIND_FUNC, .name = "Reset Event Settings",
        .desc = {"Restore defaults and restart in timed mode.", "Global Settings and other events are unchanged."},
        .OnSelect = Eggs_ResetSettings,
    },
    {
        .kind = OPTKIND_FUNC,
        .name = "Exit",
        .desc = { "Return to the Event Select Screen." },
        .OnSelect = Exit,
    },
};
static EventMenu Menu_Main = {
    .name = "Eggs-ercise",
    .option_num = countof(Options_Main),
    .options = Options_Main,
};

#endif // _EGGS_H_
