#include "events.h"
#include "trails.h"

typedef char trail_sample_size[(sizeof(TMTrailSample) == 40) ? 1 : -1];
static TMTrailBank bank;
static GOBJ *manager;
static unsigned local_enabled, local_mode, active_mode = TM_TRAIL_DISABLED;
static u32 native_frame;
static int native_frame_seen;
static int live;
static unsigned effective_mode(void);

void Trails_Clear(void) { TMTrail_Clear(&bank); native_frame_seen = 0; }
void Trails_Configure(unsigned enabled, unsigned mode) {
    unsigned before = effective_mode();
    local_enabled = !!enabled;
    local_mode = mode < TM_TRAIL_DISABLED ? mode : TM_TRAIL_NORMAL;
    if (before != effective_mode()) Trails_Clear();
}
static unsigned effective_mode(void) {
    return TMTrail_Effective(Settings_Get(TM_SETTING_FLAG, TM_FLAG_TRAILS_VERY_FAST),
                            Settings_Get(TM_SETTING_FLAG, TM_FLAG_TRAILS_INSTANT), local_enabled, local_mode);
}
static unsigned update_mode(void) {
    unsigned mode = effective_mode();
    if (mode != active_mode) { Trails_Clear(); active_mode = mode; }
    return mode;
}
static void add_hit(const Vec3 *a, const Vec3 *b, float size, uint32_t color, const void *source) {
    TMTrailSample sample = {
        {a->X, a->Y, a->Z}, {b->X, b->Y, b->Z}, size, color, bank.frame, (uintptr_t)source,
    };
    TMTrail_Add(&bank, &sample);
}
static void Trails_Think(GOBJ *gobj) {
    if (!live || update_mode() == TM_TRAIL_DISABLED) return;
    u32 frame = stc_match->time_frames;
    if (native_frame_seen && frame == native_frame) return;
    if (native_frame_seen && frame < native_frame) Trails_Clear();
    native_frame = frame;
    native_frame_seen = 1;
    /* Age by simulation callbacks, not skipped video frames at slower game speeds. */
    TMTrail_BeginFrame(&bank, bank.has_frame ? bank.frame + 1 : 0);
    GOBJ *fighters[12];
    uint32_t colors[12];
    unsigned count = 0;
    for (int ply = 0; ply < 6; ++ply) {
        Playerblock *player = Fighter_GetPlayerblock(ply);
        if (!player) continue;
        uint32_t color = TMTrail_PlayerColor(player->color_accent, player->p_kind == 1);
        for (int sub = 0; sub < 2; ++sub) {
            GOBJ *ft = Fighter_GetSubcharGObj(ply, sub);
            if (!ft) continue;
            FighterData *data = ft->userdata;
            fighters[count] = ft; colors[count++] = color;
            if (data->flags.dead) continue;
            for (unsigned i = 0; i < countof(data->hitbox); ++i) {
                ftHit *hit = &data->hitbox[i];
                if (hit->active) add_hit(&hit->pos_prev, &hit->pos, hit->size, color, hit);
            }
        }
    }
    for (GOBJ *gobj = (*stc_gobj_lookup)[MATCHPLINK_ITEM]; gobj; gobj = gobj->next) {
        ItemData *item = gobj->userdata;
        uint32_t color = TMTrail_PlayerColor(4, 1);
        /* Compare against live objects before using ownership; never dereference a stale owner. */
        for (unsigned i = 0; i < count; ++i)
            if (item->fighter_gobj == fighters[i]) { color = colors[i]; break; }
        for (unsigned i = 0; i < countof(item->hitbox); ++i) {
            itHit *hit = &item->hitbox[i];
            if (hit->active) add_hit(&hit->pos_prev, &hit->pos, hit->size, color, hit);
        }
    }
}
static void Trails_GX(GOBJ *gobj, int pass) {
    if (!live || pass != 2 || update_mode() == TM_TRAIL_DISABLED || !bank.has_frame) return;
    static GXColor ambient = {0, 0, 0, 0};
    for (unsigned i = 0; i < TM_TRAIL_CAPACITY; ++i) {
        TMTrailSample *sample = &bank.samples[i];
        if (sample->size <= 0 || sample->frame > bank.frame) continue;
        unsigned alpha = TMTrail_Alpha(active_mode, bank.frame - sample->frame);
        if (!alpha) continue;
        GXColor diffuse = {sample->color >> 24, sample->color >> 16, sample->color >> 8, alpha};
        Vec3 a = {sample->a.x, sample->a.y, sample->a.z};
        Vec3 b = {sample->b.x, sample->b.y, sample->b.z};
        Develop_DrawSphere(sample->size, &a, &b, &diffuse, &ambient);
    }
}
void Trails_SceneChange(void) {
    live = 0;
    manager = 0; /* Native scene cleanup owns the old GOBJ. */
    Trails_Clear();
}
void Trails_MatchStart(void) {
    if (manager) return;
    local_enabled = 0; local_mode = TM_TRAIL_NORMAL; active_mode = TM_TRAIL_DISABLED;
    Trails_Clear();
    live = 1;
    manager = GObj_Create(0, 7, 0);
    if (!manager) { live = 0; return; }
    GObj_AddProc(manager, Trails_Think, 22); /* After fighters, items and event overlay callbacks. */
    GObj_AddGXLink(manager, Trails_GX, 5, 0);
}
