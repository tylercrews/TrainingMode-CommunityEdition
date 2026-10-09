#include "events.h"
#include "osd_layout.h"

/* Allocate result objects only when a producer emits. Empty reserved cells use
 * at most nine light Texts. Native producers retain their valid GOBJ/text until
 * after their post-Message_Display edits; no encoded text-buffer surgery. */
static struct {
    GOBJ *latest[TM_OSD_KEYS];
    TMOSDHistory history[TM_OSD_KEYS];
    TMOSDMap map;
    Text *empty[TM_OSD_GRID_CELLS], *page_text;
    int empty_key[TM_OSD_GRID_CELLS], empty_mode[TM_OSD_GRID_CELLS];
    GOBJ *fighter[TM_OSD_PLAYERS];
    int spawn[TM_OSD_PLAYERS];
    unsigned frame, restore, mode;
    unsigned shown_page, shown_pages;
    int canvas, seen;
} layout;
static const char *category_names[TM_SETTINGS_OSDS] = {
    "Wavedash", "L-Cancel", "Act OoS", "Dashback", "Fighter-specific Tech",
    "Powershield", "SDI Inputs", "Lockout Timers", "Item Throw Interrupts", "Boost Grab",
    "Act OoWait", "Act OoAirborne", "Jump Cancel", "Fastfall", "Frame Advantage",
    "Combo Counter", "Grab Breakout", "Ledgedash", "Act OoHitstun",
};
static const char *slot_name(unsigned key) {
    unsigned slot = key % TM_OSD_SLOTS;
    if (slot == TM_SETTINGS_OSDS) return "JC Shine";
    if (slot == 4 && (layout.map.split_owners & (1u << (key / TM_OSD_SLOTS)))) return "Jump Out Of Shine";
    return category_names[slot];
}
static void sync_split_owners(void) {
    unsigned mask = 0;
    for (unsigned q = 0; q < TM_OSD_PLAYERS; ++q) {
        GOBJ *object = Fighter_GetSubcharGObj(q, 0);
        FighterData *data = object ? object->userdata : 0;
        if (data && (data->kind == FTKIND_FOX || data->kind == FTKIND_FALCO)) mask |= 1u << q;
    }
    if (layout.map.split_owners != mask)
        for (unsigned i = 0; i < TM_OSD_GRID_CELLS; ++i) layout.empty_key[i] = -1;
    TMOSD_MapSplitOwners(&layout.map, mask);
}
static GXColor muted = {180, 190, 205, 255};
static void bar_hidden(JOBJ *joint, int hidden) {
    if(hidden)joint->flags |= JOBJ_HIDDEN; else joint->flags &= ~JOBJ_HIDDEN;
    for(JOBJ *child=joint->child;child;child=child->sibling)bar_hidden(child,hidden);
}
static unsigned current_mode(void) { return Settings_Get(TM_SETTING_OSD_LAYOUT, 0); }
static unsigned eligible(void) {
    unsigned owners = 0;
    int hide_cpu = Settings_Get(TM_SETTING_FLAG, TM_FLAG_CPU_OSDS_OFF);
    for (unsigned q = 0; q < TM_OSD_PLAYERS; ++q) {
        Playerblock *player = Fighter_GetPlayerblock(q);
        if (player && player->p_kind <= 1 && (!hide_cpu || player->p_kind != 1) && Fighter_GetSubcharGObj(q, 0)) owners |= 1u << q;
    }
    return owners;
}
static void hide_empty(void) {
    for (unsigned i = 0; i < TM_OSD_GRID_CELLS; ++i) if (layout.empty[i]) layout.empty[i]->hidden = 1;
    if (layout.page_text) layout.page_text->hidden = 1;
}
void Message_LayoutInit(int canvas) {
    /* Scene teardown owns all old objects/canvases; do not dereference stale
     * previous-scene pointers when OnStartMelee installs the new manager. */
    memset(&layout, 0, sizeof(layout));
    for (unsigned i = 0; i < TM_OSD_GRID_CELLS; ++i) layout.empty_key[i] = -1;
    layout.canvas = canvas;
    layout.mode = current_mode();
    layout.shown_page = layout.shown_pages = ~0u;
    TMOSD_MapReset(&layout.map, Settings_Get(TM_SETTING_OSD_MASK, 0));
}
void Message_LayoutClear(void) {
    for (unsigned key = 0; key < TM_OSD_KEYS; ++key) {
        if (layout.latest[key]) Message_FreeObject(layout.latest[key]);
        layout.latest[key] = 0;
    }
    memset(layout.history, 0, sizeof(layout.history));
    layout.seen = 0; /* A fresh result after restore must not be cleared a second time. */
    hide_empty();
}
static void observe_owner(unsigned q) {
    GOBJ *fighter = Fighter_GetSubcharGObj(q, 0);
    FighterData *data = fighter ? fighter->userdata : 0;
    if (layout.fighter[q] && (layout.fighter[q] != fighter || !data ||
            layout.spawn[q] != data->spawn_num || data->flags.dead)) {
        for (unsigned slot = 0; slot < TM_OSD_SLOTS; ++slot) {
            unsigned key = q * TM_OSD_SLOTS + slot;
            if (layout.latest[key]) Message_FreeObject(layout.latest[key]);
            layout.latest[key] = 0; memset(&layout.history[key], 0, sizeof(layout.history[key]));
        }
    }
    layout.fighter[q] = fighter; layout.spawn[q] = data ? data->spawn_num : 0;
}
int Message_LayoutAdd(GOBJ *object, int queue) {
    MsgData *msg = object->userdata;
    if (!current_mode()) return 0;
    int key = TMOSD_MessageKey(queue, msg->settings_id, msg->kind);
    if (key < 0 || !Settings_Get(TM_SETTING_OSD_COLOR, msg->settings_id)) return 0;
    observe_owner(queue); /* Clear the previous generation before installing its new result. */
    Playerblock *player = Fighter_GetPlayerblock(queue);
    if (player && player->p_kind == 1 && Settings_Get(TM_SETTING_FLAG, TM_FLAG_CPU_OSDS_OFF)) return 0;
    if (layout.latest[key] && layout.latest[key] != object) {
        MsgData *old = layout.latest[key]->userdata;
        /* Capture finalized metadata before replacement, even if two genuine
         * attempts were emitted between manager updates. */
        if (!old->layout_captured) {
            TMOSD_HistoryPush(&layout.history[key], old->kind, old->timing_frame,
                old->timing_second_turn || old->timing_failed ? 0xFFA2BAFF : OSD_TimingColorFor(old->timing_frame, old->timing_best),
                old->timing_failed ? 2 : old->timing_second_turn, old->native_frame);
        }
        Message_FreeObject(layout.latest[key]);
    }
    layout.latest[key] = object;
    layout.mode = current_mode();
    msg->layout_owned = 1; msg->layout_key = key;
    msg->state = MSGSTATE_WAIT; msg->anim_timer = 0;
    /* Position immediately too, for producers scheduled after priority 21. */
    TMOSD_MapOwners(&layout.map, eligible());
    sync_split_owners();
    Message_LayoutGeometry(object);
    return 1;
}
int Message_LayoutImport(GOBJ *object, int queue) {
    if (!current_mode()) return 0;
    MsgData *incoming = object->userdata;
    int key = TMOSD_MessageKey(queue, incoming->settings_id, incoming->kind);
    MsgData *previous = key >= 0 && layout.latest[key] ? layout.latest[key]->userdata : 0;
    if (previous && layout.latest[key] != object && (previous->native_frame > incoming->native_frame ||
            (previous->native_frame == incoming->native_frame && previous->layout_owned == 1))) {
        /* Only queued, fully finalized objects enter here. Never destroy a
         * newly returned Message_Display object before its caller's edits. */
        Message_FreeObject(object);
        return 1;
    }
    int imported = Message_LayoutAdd(object, queue);
    if (imported) incoming->layout_owned = 2;
    return imported;
}
int Message_LayoutVisible(MsgData *msg) {
    unsigned mode = current_mode();
    if (!mode) return !msg->layout_owned;
    if (msg->settings_id < 0 || (unsigned)msg->queue_num >= TM_OSD_PLAYERS) return 1;
    if (!msg->layout_owned) return 0;
    TMOSDCell cell = TMOSD_Cell(&layout.map, msg->layout_key, mode);
    return cell.key >= 0 && cell.page == (int)Settings_Get(TM_SETTING_OSD_PAGE, 0);
}
void Message_LayoutGeometry(GOBJ *object) {
    MsgData *msg = object->userdata;
    if (!msg->layout_owned) return;
    unsigned mode = current_mode();
    TMOSDCell cell = TMOSD_Cell(&layout.map, msg->layout_key, mode);
    if (cell.key < 0) return;
    Text *text = msg->text;
    JOBJ *joint = object->hsd_object;
    if (mode == TM_OSD_PANEL) {
        text->align = 0; text->use_aspect = 0;
        text->viewport_scale.X = text->viewport_scale.Y = 0.027f;
        text->trans.X = cell.x; text->trans.Y = -cell.y;
        unsigned detail = 0;
        const char *title = msg->layout_title[0] ? msg->layout_title : slot_name(msg->layout_key);
        float detail_base = strlen(title) > 13 ? 28.f : 0.f;
        for (int i = 0; i < msg->line_count; ++i) {
            if (i == 0 || (msg->timing_frame >= 0 && i == msg->timing_subtext))
                Text_SetPosition(text, i, -2000, 0);
            else Text_SetPosition(text, i, 0, detail_base + detail++ * 26.f);
            Text_SetScale(text, i, 1.f, 1.f);
        }
        joint->scale.X = 5.8f; joint->scale.Y = 1.98f; joint->scale.Z = MSGJOINT_SCALE;
        joint->trans.X = cell.x + 9.4f; joint->trans.Y = cell.y - 0.15f;
    } else {
        joint->scale.X = 2.65f; joint->scale.Y = joint->scale.Z = MSGJOINT_SCALE;
        joint->trans.X = cell.x; joint->trans.Y = cell.y;
        text->align = 1; text->use_aspect = 1; text->aspect.X = 215;
        text->viewport_scale.X = text->viewport_scale.Y = MSGJOINT_SCALE * .01f * MSGTEXT_BASESCALE;
        text->trans.X = cell.x; text->trans.Y = -cell.y + MSGTEXT_BASEY * MSGJOINT_SCALE * .25f;
        for (int i = 0; i < msg->line_count; ++i) {
            Text_SetPosition(text, i, 0, -MSGTEXT_YOFFSET + i * MSGTEXT_YOFFSET);
            Text_SetScale(text, i, 1.f, 1.f);
        }
    }
    /* Stable layouts have no shrinking lifetime bar or collapsing background. */
    JOBJ *bar;
    JOBJ_GetChild(joint, &bar, 4, -1);
    if (bar) bar_hidden(bar,1);
    JOBJ_SetMtxDirtySub(joint);
    if (msg->layout_footer) {
        msg->layout_footer->trans.X = cell.x;
        msg->layout_footer->trans.Y = -cell.y + (mode == TM_OSD_PANEL ? 0 : 2.5f);
        msg->layout_footer->align = mode == TM_OSD_PANEL ? 0 : 1;
    }
    if (msg->layout_timing) {
        msg->layout_timing->trans.X = cell.x + 9.375f;
        msg->layout_timing->trans.Y = -cell.y - .3f;
    }
}
void Message_LayoutRecent(void) {
    hide_empty();
    for (unsigned key = 0; key < TM_OSD_KEYS; ++key) {
        GOBJ *object = layout.latest[key];
        if (!object) continue;
        MsgData *msg = object->userdata;
        /* Restore native row geometry before returning to Recent. */
        msg->text->align = 1; msg->text->use_aspect = 1; msg->text->aspect.X = MSGTEXT_BASEWIDTH;
        msg->text->viewport_scale.X = msg->text->viewport_scale.Y = MSGJOINT_SCALE * .01f * MSGTEXT_BASESCALE;
        JOBJ *joint = object->hsd_object;
        joint->scale.X = joint->scale.Y = joint->scale.Z = MSGJOINT_SCALE;
        JOBJ *bar;
        JOBJ_GetChild(joint, &bar, 4, -1);
        if (bar) bar_hidden(bar,0);
        for (int i = 0; i < msg->line_count; ++i) {
            Text_SetPosition(msg->text, i, 0, (msg->line_count - 1) * (-MSGTEXT_YOFFSET / 2) + i * MSGTEXT_YOFFSET);
            Text_SetScale(msg->text, i, 1.f, 1.f);
        }
        if (msg->layout_footer) { Text_Destroy(msg->layout_footer); msg->layout_footer = 0; }
        if (msg->layout_timing) { Text_Destroy(msg->layout_timing); msg->layout_timing = 0; }
        layout.latest[key] = 0;
        msg->layout_owned = 0; msg->layout_key = -1;
        msg->layout_captured = 0;
        msg->state = MSGSTATE_WAIT; msg->anim_timer = 0;
        Message_Add(object, key / TM_OSD_SLOTS);
    }
    memset(layout.history, 0, sizeof(layout.history));
}
unsigned Message_LayoutPageCount(void) { return TMOSD_PageCount(&layout.map, current_mode()); }
static void panel_title(Text *text, const char *title) {
    char first[33], second[33];
    unsigned length = strlen(title), split = length;
    if (length > 13) {
        split = 0;
        for (unsigned i = 1; i <= 13; ++i) if (title[i] == ' ') split = i;
        if (!split) split = 13;
    }
    memcpy(first, title, split); first[split] = 0;
    const char *tail = title + split; if (*tail == ' ') ++tail;
    strcpy(second, tail);
    Text_SetText(text, 0, "%s", first); Text_SetText(text, 1, "%s", second);
}
static void footer(GOBJ *object, unsigned mode) {
    MsgData *msg = object->userdata;
    TMOSDHistory *history = &layout.history[msg->layout_key];
    if (!msg->layout_captured) {
        TMOSD_HistoryPush(history, msg->kind, msg->timing_frame,
            msg->timing_second_turn || msg->timing_failed ? 0xFFA2BAFF : OSD_TimingColorFor(msg->timing_frame, msg->timing_best),
            msg->timing_failed ? 2 : msg->timing_second_turn, msg->native_frame);
        msg->layout_captured = 1;
    }
    if (!Message_LayoutVisible(msg)) {
        if (msg->layout_footer) { Text_Destroy(msg->layout_footer); msg->layout_footer = 0; }
        if (msg->layout_timing) { Text_Destroy(msg->layout_timing); msg->layout_timing = 0; }
        return;
    }
    if (mode != TM_OSD_PANEL) {
        if (msg->layout_footer) { Text_Destroy(msg->layout_footer); msg->layout_footer = 0; }
        if (msg->layout_timing) { Text_Destroy(msg->layout_timing); msg->layout_timing = 0; }
        return; /* Fixed Grid has no New/Last or ownership status line. */
    }
    int state = mode * 4;
    if (msg->layout_footer && msg->layout_footer_state >= 0 && msg->layout_footer_state / 4 != (int)mode) {
        Text_Destroy(msg->layout_footer); msg->layout_footer = 0;
    }
    if (!msg->layout_footer) {
        msg->layout_footer = Text_CreateText(2, layout.canvas);
        msg->layout_footer->kerning = 1; msg->layout_footer->use_aspect = 1;
        msg->layout_footer->aspect.X = mode == TM_OSD_PANEL ? 710 : 450;
        msg->layout_footer->viewport_scale.X = msg->layout_footer->viewport_scale.Y = .023f;
        msg->layout_footer->color = muted;
        unsigned rows = 9;
        for (unsigned i = 0; i < rows; ++i) Text_AddSubtext(msg->layout_footer, 0, 0, "");
        msg->layout_footer_state = -1;
    }
    if (msg->layout_footer_state != state) {
        char line[128], part[24];
        unsigned used = 0;
        for (const char *p = part; *p; ++p) line[used++] = *p;
        if (mode == TM_OSD_PANEL) {
            Text_SetText(msg->layout_footer, 7, "");
            const char *title = msg->layout_title[0] ? msg->layout_title : slot_name(msg->layout_key);
            panel_title(msg->layout_footer, title);
            Text_SetText(msg->layout_footer, 2, ""); Text_SetText(msg->layout_footer, 3, "");
            if (msg->timing_frame >= 0) {
                if (!msg->layout_timing) {
                    msg->layout_timing = Text_CreateText(2, layout.canvas);
                    msg->layout_timing->kerning = 1; msg->layout_timing->use_aspect = 1;
                    msg->layout_timing->aspect.X = 330;
                    msg->layout_timing->viewport_scale.X = msg->layout_timing->viewport_scale.Y = .025f;
                    Text_AddSubtext(msg->layout_timing, 0, 0, "");
                }
                OSD_TimingText(msg, line, 0); /* Prefix and result always share one font scale and native row. */
                Text_SetText(msg->layout_timing, 0, "%s", line);
                Text_SetScale(msg->layout_timing, 0, 2.2f, 2.2f);
            }
            static const char hex[] = "0123456789ABCDEF";
            for (unsigned row = 0; row < 3; ++row) {
                if (history->count <= row + 1) { Text_SetText(msg->layout_footer, 4 + row, ""); continue; }
                unsigned i = history->count - 2 - row; used = 0;
                line[used++] = 0x1B;
                for (int shift = 28; shift >= 0; shift -= 4) line[used++] = hex[(history->color[i] >> shift) & 15];
                if (history->turn[i] == 2) strcpy(part, "FAIL");
                else sprintf(part, history->turn[i] ? "%dtrn" : "%df", history->frame[i]);
                for (const char *p = part; *p; ++p) line[used++] = *p;
                line[used] = 0; Text_SetText(msg->layout_footer, 4 + row, "%s", line);
            }
            Text_SetText(msg->layout_footer, 8, "|");
        } else {
            line[used] = 0;
            Text_SetText(msg->layout_footer, 0, "%s", line);
        }
        msg->layout_footer_state = state;
    }
    Text *view = msg->layout_footer;
    view->use_aspect = mode == TM_OSD_PANEL ? 0 : 1;
    view->viewport_scale.X = view->viewport_scale.Y = mode == TM_OSD_PANEL ? .025f : .023f;
    if (mode == TM_OSD_PANEL) {
        unsigned rgba = OSD_PaletteColor(Settings_Get(TM_SETTING_OSD_COLOR, msg->settings_id));
        GXColor title = {rgba >> 24, rgba >> 16, rgba >> 8, rgba};
        Text_SetColor(view, 0, &title); Text_SetColor(view, 1, &title);
        Text_SetPosition(view, 0, 0, -35); Text_SetPosition(view, 1, 0, 0);
        Text_SetScale(view, 0, 1.05f, 1.05f); Text_SetScale(view, 1, 1.05f, 1.05f);
        Text_SetPosition(view, 2, 445, -12); Text_SetScale(view, 2, 2.2f, 2.2f);
        Text_SetPosition(view, 3, 425, 35); Text_SetScale(view, 3, .6f, .6f);
        for (unsigned row = 0; row < 3; ++row) {
            Text_SetPosition(view, 4 + row, 730, -28.f + row * 28.f);
            Text_SetScale(view, 4 + row, .8f, .8f);
        }
        Text_SetPosition(view, 7, 0, 60); Text_SetScale(view, 7, .5f, .5f);
        Text_SetPosition(view, 8, 325, 0); Text_SetScale(view, 8, .6f, 2.3f);
    } else view->aspect.X = 450;
    msg->layout_footer->hidden = 0;
    if (msg->layout_timing) msg->layout_timing->hidden = 0;
}
void Message_LayoutUpdate(void) {
    unsigned mode = current_mode();
    if (!mode) { if (layout.mode) Message_LayoutRecent(); layout.mode = mode; return; }
    uint32_t mask = Settings_Get(TM_SETTING_OSD_MASK, 0);
    if (layout.map.mask != mask) {
        TMOSD_MapReset(&layout.map, mask);
        for (unsigned key = 0; key < TM_OSD_KEYS; ++key) if (!(mask & (1u << TMOSD_Category(key)))) {
            if (layout.latest[key]) Message_FreeObject(layout.latest[key]);
            layout.latest[key] = 0; memset(&layout.history[key], 0, sizeof(layout.history[key]));
        }
    }
    TMOSD_MapOwners(&layout.map, eligible());
    sync_split_owners();
    unsigned serial = event_vars && event_vars->get_restore_serial ? event_vars->get_restore_serial() : 0;
    unsigned frame = stc_match->time_frames;
    if (layout.seen && (frame < layout.frame || serial != layout.restore)) Message_LayoutClear();
    int tick = !layout.seen || frame != layout.frame;
    layout.frame = frame; layout.restore = serial; layout.seen = 1; layout.mode = mode;
    for (unsigned q = 0; q < TM_OSD_PLAYERS; ++q) observe_owner(q);
    unsigned pages = Message_LayoutPageCount(), page = Settings_Get(TM_SETTING_OSD_PAGE, 0);
    if (page >= pages) { page = pages - 1; Settings_Set(TM_SETTING_OSD_PAGE, 0, page); }
    hide_empty();
    for (unsigned key = 0; key < TM_OSD_KEYS; ++key) {
        GOBJ *object = layout.latest[key];
        if (object) {
            MsgData *msg = object->userdata;
            if (tick && msg->alive_timer < MSG_LIFETIME && Pause_CheckStatus(1) != 2) ++msg->alive_timer;
            footer(object, mode); Message_LayoutGeometry(object);
        }
        TMOSDCell cell = TMOSD_Cell(&layout.map, key, mode);
        if (cell.key < 0 || cell.page != (int)page || object) continue;
        unsigned i = cell.cell;
        if (!layout.empty[i]) {
            layout.empty[i] = Text_CreateText(2, layout.canvas);
            layout.empty[i]->kerning = 1; layout.empty[i]->use_aspect = 1;
            for(unsigned row=0;row<9;++row) Text_AddSubtext(layout.empty[i],0,0,"");
        }
        Text *text = layout.empty[i];
        if (layout.empty_key[i] != (int)key || layout.empty_mode[i] != (int)mode) {
            for(unsigned row=0;row<9;++row) Text_SetText(text,row,"");
            if(mode==TM_OSD_PANEL) panel_title(text, slot_name(key));
            else Text_SetText(text, 0, "%s", slot_name(key));
            Text_SetText(text,mode==TM_OSD_PANEL ? 2 : 1,"--");
            if(mode==TM_OSD_PANEL)Text_SetText(text,8,"|");
            layout.empty_key[i] = key;
            layout.empty_mode[i] = mode;
        }
        text->align = mode == TM_OSD_PANEL ? 0 : 1;
        text->trans.X = cell.x; text->trans.Y = -cell.y;
        text->use_aspect=mode==TM_OSD_PANEL ? 0 : 1;
        text->aspect.X = 215;
        text->viewport_scale.X = text->viewport_scale.Y = mode == TM_OSD_PANEL ? .025f : .042f;
        Text_SetPosition(text, 0, 0, mode == TM_OSD_PANEL ? -29 : -MSGTEXT_YOFFSET);
        Text_SetPosition(text, 1, 0, mode == TM_OSD_PANEL ? -6 : 0);
        Text_SetPosition(text, 2, 0, mode == TM_OSD_PANEL ? 66 : 59);
        unsigned rgba = OSD_PaletteColor(Settings_Get(TM_SETTING_OSD_COLOR, TMOSD_Category(key)));
        GXColor color = {rgba >> 24, rgba >> 16, rgba >> 8, rgba};
        Text_SetColor(text, 0, &color); Text_SetColor(text, 1, mode==TM_OSD_PANEL ? &color : &muted); Text_SetColor(text, 2, &muted);
        for(unsigned row=0;row<9;++row) Text_SetScale(text,row,1,1);
        if(mode==TM_OSD_PANEL){
            Text_SetPosition(text,0,0,-35); Text_SetPosition(text,1,0,0);
            Text_SetScale(text,0,1.05f,1.05f);Text_SetScale(text,1,1.05f,1.05f);
            Text_SetPosition(text,2,445,-12);Text_SetScale(text,2,2.2f,2.2f);
            Text_SetPosition(text,7,0,60);Text_SetScale(text,7,.5f,.5f);
            Text_SetPosition(text,8,325,0);Text_SetScale(text,8,.6f,2.3f);
        }
        Playerblock *owner = Fighter_GetPlayerblock(key / TM_OSD_SLOTS);
        text->hidden = Settings_Get(TM_SETTING_FLAG, TM_FLAG_OSDS_OFF) ||
            (owner && owner->p_kind == 1 && Settings_Get(TM_SETTING_FLAG, TM_FLAG_CPU_OSDS_OFF));
    }
    if (!layout.page_text) {
        layout.page_text = Text_CreateText(2, layout.canvas);
        layout.page_text->kerning = 1; layout.page_text->align = 0;
        layout.page_text->viewport_scale.X = layout.page_text->viewport_scale.Y = .026f;
        layout.page_text->color = muted;
        layout.page_text->trans.X = -27; layout.page_text->trans.Y = 12;
        Text_AddSubtext(layout.page_text, 0, 0, "");
    }
    if (layout.shown_page != page || layout.shown_pages != pages) {
        Text_SetText(layout.page_text, 0, "OSDs %d/%d - paused L/R: page", page + 1, pages);
        layout.shown_page = page; layout.shown_pages = pages;
    }
    layout.page_text->hidden = pages <= 1 || Settings_Get(TM_SETTING_FLAG, TM_FLAG_OSDS_OFF);
}
void Message_LayoutEndCombo(int queue) {
    int key = TMOSD_Key(queue, 22);
    if (key < 0 || !layout.latest[key]) return;
    MsgData *msg = layout.latest[key]->userdata;
    GXColor green = {141, 255, 110, 255};
    Text_SetColor(msg->text, 1, &green);
    msg->alive_timer = MSG_LIFETIME;
}
