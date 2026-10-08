/* Native Training Lab renderer/navigation. Included only by menu.c. */
#include "menu_controller.h"
#define MC_PREVIEW_LINES 24
#define MC_DESC_LINES 8
#define MC_ROW_Y 9.9f
#define MC_ROW_STEP 2.4f

static int MC_Enabled(MenuData *data) { return data->root_menu && data->root_menu->tab_num; }
static int MC_HasSelector(MenuData *data) { return !data->curr_menu->prev || data->curr_menu->page_num; }
static int MC_ValueCount(EventOption *option) { return option->kind == OPTKIND_TOGGLE ? 2 : option->value_num; }
static int MC_ValueMinimum(EventOption *option) { return option->kind == OPTKIND_TOGGLE ? 0 : option->value_min; }
static int MC_Index(MenuData *data) { return data->curr_menu->cursor + data->curr_menu->scroll; }
static void MC_Select(MenuData *data, int index)
{
    EventMenu *menu = data->curr_menu;
    menu->scroll = MenuController_Scroll(menu->scroll, index, EventMenu_OptionCount(menu), MENU_MAXOPTION);
    menu->cursor = index - menu->scroll;
}
static void MC_Tab(MenuData *data, int index)
{
    data->tab = index;
    data->curr_menu = data->root_menu->tabs[index].menu;
    data->curr_menu->prev = 0;
    data->curr_menu->shortcuts = data->root_menu->shortcuts;
    data->selector = 1;
    data->picker = 0;
}
static JOBJ *MC_Box(GOBJ *gobj, float x, float y, float width, float height, GXColor color, float alpha)
{
    JOBJ *box = JOBJ_LoadJoint(event_vars->menu_assets->popup);
    JOBJ *corners[4];
    JOBJ_AddChild(gobj->hsd_object, box);
    JOBJ_GetChild(box, corners, 2, 3, 4, 5, -1);
    for (int i = 0; i < 4; ++i) {
        corners[i]->trans.X = x + ((i & 1) ? width / 2 : -width / 2);
        corners[i]->trans.Y = y + (i < 2 ? height / 2 : -height / 2);
    }
    DOBJ_SetFlags(box->dobj, DOBJ_HIDDEN);
    box->dobj->next->mobj->mat->diffuse = color;
    box->dobj->next->mobj->mat->alpha = alpha;
    return box;
}
static void MC_CreateModel(GOBJ *gobj)
{
    MenuData *data = gobj->userdata;
    JOBJ *root = JOBJ_LoadJoint(event_vars->menu_assets->menu);
    GObj_AddObject(gobj, 3, root);
    GObj_DestroyGXLink(gobj);
    GObj_AddGXLink(gobj, EventMenu_MenuGX, GXLINK_MENUMODEL, GXPRI_MENUMODEL);
    MC_Box(gobj, -11.2f, -.6f, 21.6f, 28.2f, (GXColor){19, 24, 36, 255}, .95f);
    MC_Box(gobj, 11.f, -.6f, 21.6f, 28.2f, (GXColor){27, 33, 47, 255}, .95f);
    for (int i = 0; i < MENU_MAXOPTION; ++i)
        data->rowboxes[i] = MC_Box(gobj, -11.2f, MC_ROW_Y - i * MC_ROW_STEP, 20.8f, 2.25f, (GXColor){30, 38, 53, 255}, .85f);
    data->highlight_menu = MC_Box(gobj, -11.2f, MC_ROW_Y, 20.8f, 2.25f, (GXColor)MENUHIGHLIGHT_COLOR, .55f);
    for (int i = 0; i < data->root_menu->tab_num; ++i)
        data->tabboxes[i] = MC_Box(gobj, -19.1f + i * 5.45f, 16.1f, 5.2f, 2.7f, (GXColor){30, 38, 53, 255}, .95f);
}
static Text *MC_Text(MenuData *data, float width)
{
    Text *text = Text_CreateText(2, data->canvas_menu);
    text->gobj->gx_cb = EventMenu_TextGX;
    text->kerning = 1;
    text->use_aspect = 1;
    text->aspect.X = width;
    text->viewport_scale.X = text->viewport_scale.Y = MENU_CANVASSCALE;
    return text;
}
static void MC_Subtext(Text *text, float x, float y, float scale)
{
    int line = Text_AddSubtext(text, x, y, "");
    Text_SetScale(text, line, scale, scale);
}
static void MC_CreateText(GOBJ *gobj)
{
    MenuData *data = gobj->userdata;
    data->text_title = MC_Text(data, 870);
    MC_Subtext(data->text_title, -425, -390, 1.35f);
    MC_Subtext(data->text_title, -416, -270, .9f);
    MC_Subtext(data->text_title, 25, -270, .85f);
    data->text_tabs = MC_Text(data, 102);
    data->text_tabs->align = 1;
    for (int i = 0; i < data->root_menu->tab_num; ++i) MC_Subtext(data->text_tabs, -382 + i * 109, -337, .72f);
    data->text_name = MC_Text(data, 250);
    data->text_value = MC_Text(data, 136);
    data->text_value->align = 1;
    for (int i = 0; i < MENU_MAXOPTION; ++i) {
        MC_Subtext(data->text_name, -416, -211 + i * 48, .78f);
        MC_Subtext(data->text_value, -76, -211 + i * 48, .72f);
    }
    data->text_desc = MC_Text(data, 390);
    for (int i = 0; i < MC_DESC_LINES; ++i) MC_Subtext(data->text_desc, 25, -218 + i * 22, .78f);
    data->text_preview = MC_Text(data, 390);
    for (int i = 0; i < MC_PREVIEW_LINES; ++i) MC_Subtext(data->text_preview, 25, -112 + i * 17, .65f);
    data->text_hints = MC_Text(data, 870);
    MC_Subtext(data->text_hints, -416, 321, .7f);
    MC_Subtext(data->text_hints, -416, 347, .65f);
}
static int MC_Description(Text *text, const char *const source[MENU_DESCLINEMAX])
{
    char paragraph[384];
    int used = 0;
    for (int i = 0; i < MENU_DESCLINEMAX; ++i) {
        const char *s = source[i];
        if (!s) continue;
        if (used && used < (int)sizeof(paragraph) - 1) paragraph[used++] = ' ';
        while (*s && used < (int)sizeof(paragraph) - 1) paragraph[used++] = *s++;
    }
    paragraph[used] = 0;
    int at = 0;
    int lines = 0;
    for (int i = 0; i < MC_DESC_LINES; ++i) {
        char line[43];
        while (paragraph[at] == ' ') ++at;
        int length = 0;
        while (paragraph[at + length] && length < 42) ++length;
        if (length == 42 && paragraph[at + length]) {
            int word = length;
            while (word && paragraph[at + word] != ' ') --word;
            if (word) length = word;
        }
        for (int j = 0; j < length; ++j) line[j] = paragraph[at + j];
        line[length] = 0;
        Text_SetText(text, i, "%s", line);
        if (length) lines = i + 1;
        at += length;
    }
    return lines;
}
static void MC_PreviewRow(MenuData *data, EventOption *option, int *line)
{
    if (!option || !option->name || !option->name[0] || *line >= MC_PREVIEW_LINES) return;
    Text_SetText(data->text_preview, (*line)++, "%s%s%s", option->name,
                 option->kind == OPTKIND_MENU ? " >" : "", option->disable ? " (unavailable)" : "");
}
static void MC_Preview(MenuData *data, EventMenu *menu, int page_only)
{
    int line = 0;
    for (int i = 0; i < MC_PREVIEW_LINES; ++i) Text_SetText(data->text_preview, i, "");
    if (!menu) return;
    Text_SetText(data->text_preview, line++, "Included options:");
    if (menu->page_num) {
        for (int page = 0; page < menu->page_num; ++page) {
            if (page_only && page != menu->page) continue;
            if (line < MC_PREVIEW_LINES) Text_SetText(data->text_preview, line++, "[%s]", menu->pages[page].name);
            for (int i = 0; i < menu->pages[page].option_num; ++i) MC_PreviewRow(data, menu->pages[page].rows[i].option, &line);
        }
    } else {
        for (int i = 0; i < EventMenu_OptionCount(menu); ++i) MC_PreviewRow(data, EventMenu_GetOption(menu, i), &line);
    }
}
static GXColor MC_RowColor(int group, int index)
{
    GXColor color = {30, 38, 53, 255};
    if (group == 1) color = (GXColor){24, 49, 63, 255};
    if (group == 2) color = (GXColor){28, 55, 43, 255};
    if (group == 3) color = (GXColor){48, 37, 64, 255};
    if (group == 4) color = (GXColor){57, 48, 29, 255};
    if (index & 1) { color.r += 8; color.g += 8; color.b += 8; }
    return color;
}
static void MC_UpdateText(GOBJ *gobj)
{
    MenuData *data = gobj->userdata;
    EventMenu *menu = data->curr_menu;
    int count = EventMenu_OptionCount(menu);
    int index = MC_Index(data);
    if (index >= count) { index = count ? count - 1 : 0; MC_Select(data, index); }
    EventOption *option = EventMenu_GetOption(menu, index);
    GXColor white = {255, 255, 255, 255}, gray = {145, 151, 162, 255}, black = {28, 25, 18, 255};
    Text_SetText(data->text_title, 0, "Training Lab");
    Text_SetText(data->text_title, 1, "%s%s%s", menu->name, menu->page_num ? " / " : "", menu->page_num ? menu->pages[menu->page].name : "");
    Text_SetText(data->text_title, 2, "%s", data->selector ? menu->name : option && option->name ? option->name : "");
    for (int i = 0; i < data->root_menu->tab_num; ++i) {
        int active = data->tab == i;
        Text_SetText(data->text_tabs, i, "%s%s", active && data->selector && !menu->prev ? "> " : "", data->root_menu->tabs[i].name);
        Text_SetColor(data->text_tabs, i, active ? &black : &white);
        data->tabboxes[i]->dobj->next->mobj->mat->diffuse = active ? (GXColor)MENUHIGHLIGHT_COLOR : (GXColor){30, 38, 53, 255};
    }
    if (data->picker) {
        int relative = data->picker_value - MC_ValueMinimum(data->picker);
        data->picker_scroll = MenuController_Scroll(data->picker_scroll, relative, MC_ValueCount(data->picker), MENU_MAXOPTION);
    }
    for (int i = 0; i < MENU_MAXOPTION; ++i) {
        int row = data->picker ? data->picker_scroll + i : menu->scroll + i;
        EventOption *entry = data->picker ? data->picker : EventMenu_GetOption(menu, row);
        int exists = entry && entry->name && entry->name[0] && (!data->picker || row < MC_ValueCount(entry));
        if (!exists) {
            Text_SetText(data->text_name, i, ""); Text_SetText(data->text_value, i, "");
            JOBJ_SetFlags(data->rowboxes[i], JOBJ_HIDDEN); continue;
        }
        JOBJ_ClearFlags(data->rowboxes[i], JOBJ_HIDDEN);
        data->rowboxes[i]->dobj->next->mobj->mat->diffuse = MC_RowColor(data->picker ? 0 : EventMenu_OptionGroup(menu, row), row);
        Text_SetColor(data->text_name, i, entry->disable ? &gray : &white);
        Text_SetColor(data->text_value, i, entry->disable ? &gray : &white);
        if (data->picker) {
            int value = row + MC_ValueMinimum(entry);
            if (entry->kind == OPTKIND_STRING) Text_SetText(data->text_name, i, "%s", entry->values[value]);
            else if (entry->kind == OPTKIND_TOGGLE) Text_SetText(data->text_name, i, "%s", value ? "On" : "Off");
            else Text_SetText(data->text_name, i, entry->format, value);
            Text_SetText(data->text_value, i, "%s", value == entry->val ? "Current" : "");
        } else {
            Text_SetText(data->text_name, i, "%s", entry->name);
            if (entry->kind == OPTKIND_STRING) Text_SetText(data->text_value, i, "%s", entry->values[entry->val]);
            else if (entry->kind == OPTKIND_INT) Text_SetText(data->text_value, i, entry->format, entry->val);
            else if (entry->kind == OPTKIND_TOGGLE) Text_SetText(data->text_value, i, "%s", entry->val ? "On" : "Off");
            else if (entry->kind == OPTKIND_MENU) Text_SetText(data->text_value, i, ">");
            else Text_SetText(data->text_value, i, "%s", entry->value_string ? entry->value_string : entry->kind == OPTKIND_INFO ? "" : "A");
        }
    }
    if (data->selector && !data->picker) JOBJ_SetFlags(data->highlight_menu, JOBJ_HIDDEN);
    else {
        JOBJ_ClearFlags(data->highlight_menu, JOBJ_HIDDEN);
        int cursor = data->picker ? data->picker_value - MC_ValueMinimum(data->picker) - data->picker_scroll : menu->cursor;
        data->highlight_menu->trans.Y = -cursor * MC_ROW_STEP;
    }
    const char *description[MENU_DESCLINEMAX] = {0};
    if (data->selector) description[0] = menu->purpose ? menu->purpose : "Choose a page, then press A or Down to enter its options.";
    else if (option) for (int i = 0; i < MENU_DESCLINEMAX; ++i) description[i] = option->desc[i];
    int description_lines = MC_Description(data->text_desc, description);
    float preview_y = -218 + max(4, description_lines) * 22 + 18;
    for (int i = 0; i < MC_PREVIEW_LINES; ++i) Text_SetPosition(data->text_preview, i, 25, preview_y + i * 17);
    MC_Preview(data, data->picker ? 0 : data->selector ? menu : option && option->kind == OPTKIND_MENU ? option->menu : 0, data->selector);
    if (data->picker) {
        Text_SetText(data->text_hints, 0, "Up/Down: Choose   A: Apply   B: Cancel");
        Text_SetText(data->text_hints, 1, "Choice %d/%d (current value is unchanged until A)", data->picker_value - MC_ValueMinimum(data->picker) + 1, MC_ValueCount(data->picker));
    } else if (data->selector) {
        Text_SetText(data->text_hints, 0, "Left/Right: %s   A/Down: Options   B: %s", menu->page_num ? "Page" : "Tab", menu->prev ? "Back" : "Resume");
        Text_SetText(data->text_hints, 1, "Start: Resume   Hold Y: Existing shortcuts   L/R: OSD page");
    } else {
        Text_SetText(data->text_hints, 0, "Up/Down: Move   Left/Right: Value   A: Open/Choose   B: %s", menu->prev ? "Back" : "Tabs");
        Text_SetText(data->text_hints, 1, "%s", option && option->disable ? "Unavailable in this fighter, recording or mode context." : data->tab == 4 ? "Global settings are saved and apply across events." : "Start: Resume   Hold Y: Existing shortcuts   L/R: OSD page");
    }
    JOBJ_SetMtxDirtySub(gobj->hsd_object);
}
static void MC_Think(GOBJ *gobj)
{
    MenuData *data = gobj->userdata;
    EventMenu *menu = data->curr_menu;
    HSD_Pad *pad = PadGetMaster(data->controller_index);
    int inputs = pad->down; /* One movement per press/deflection, no rapid repeat. */
    int vertical = inputs & (HSD_BUTTON_UP | HSD_BUTTON_DPAD_UP) ? -1 : inputs & (HSD_BUTTON_DOWN | HSD_BUTTON_DPAD_DOWN) ? 1 : 0;
    int horizontal = inputs & (HSD_BUTTON_LEFT | HSD_BUTTON_DPAD_LEFT) ? -1 : inputs & (HSD_BUTTON_RIGHT | HSD_BUTTON_DPAD_RIGHT) ? 1 : 0;
    EventOption *option = EventMenu_SelectedOption(data);
    if (data->picker) {
        if (inputs & HSD_BUTTON_B) data->picker = 0;
        else if (inputs & HSD_BUTTON_A) {
            EventOption *target = data->picker; int value = data->picker_value;
            data->picker = 0; EventMenu_ChangeOptionVal(gobj, target, value); return;
        } else if (vertical) {
            int count = data->picker->kind == OPTKIND_TOGGLE ? 2 : data->picker->value_num;
            int minimum = data->picker->kind == OPTKIND_TOGGLE ? 0 : data->picker->value_min;
            data->picker_value = minimum + MenuController_Move(data->picker_value - minimum, count, 0, vertical);
        } else return;
    } else if (inputs & HSD_BUTTON_B) {
        if (menu->prev) { EventMenu_PrevMenu(gobj); return; }
        if (data->selector) { EventMenu_ExitMenu(gobj); return; }
        data->selector = 1;
    } else if (vertical) {
        int count = EventMenu_OptionCount(menu), index = data->selector ? -1 : MC_Index(data);
        for (int tries = 0; tries <= count; ++tries) {
            index = MenuController_Move(index, count, MC_HasSelector(data), vertical);
            EventOption *next = EventMenu_GetOption(menu, index);
            if (index == -1 || (next && next->name && next->name[0])) break;
        }
        data->selector = index == -1;
        if (index >= 0) MC_Select(data, index);
    } else if (horizontal) {
        if (data->selector) {
            if (menu->page_num) {
                menu->page = (menu->page + menu->page_num + horizontal) % menu->page_num;
                menu->cursor = menu->scroll = 0;
            } else MC_Tab(data, (data->tab + data->root_menu->tab_num + horizontal) % data->root_menu->tab_num);
        } else if (option && !option->disable && (option->kind == OPTKIND_STRING || option->kind == OPTKIND_INT || option->kind == OPTKIND_TOGGLE)) {
            int count = option->kind == OPTKIND_TOGGLE ? 2 : option->value_num;
            int minimum = option->kind == OPTKIND_TOGGLE ? 0 : option->value_min;
            int value = MenuController_Step(option->val, minimum, count, horizontal);
            if (value == option->val) return;
            EventMenu_ChangeOptionVal(gobj, option, value); return;
        } else return;
    } else if (inputs & HSD_BUTTON_A) {
        if (data->selector) data->selector = 0;
        else if (!option || option->disable) { SFX_PlayCommon(4); return; }
        else if (option->kind == OPTKIND_MENU && option->menu) { EventMenu_NextMenu(gobj, option->menu); return; }
        else if (option->kind == OPTKIND_RESUME) { EventMenu_ExitMenu(gobj); return; }
        else if (option->kind == OPTKIND_FUNC) { EventMenu_RunFuncOption(gobj, option); return; }
        else if (option->kind == OPTKIND_STRING || option->kind == OPTKIND_INT || option->kind == OPTKIND_TOGGLE) {
            data->picker = option; data->picker_value = option->val; data->picker_scroll = 0;
        } else return;
    } else return;
    MC_UpdateText(gobj);
    SFX_PlayCommon(2);
}

/* Find a shortcut's actual place in the new tree, so B still has a meaningful
 * parent and the active tab matches its destination. Depth is bounded. */
static int MC_Path(EventMenu *menu, EventMenu *target, EventMenu **path, int depth)
{
    if (depth >= 4) return 0;
    path[depth] = menu;
    if (menu == target) return depth + 1;
    if (menu->page_num) return 0; /* Pages are terminal editors. */
    for (int i = 0; i < EventMenu_OptionCount(menu); ++i) {
        EventOption *option = EventMenu_GetOption(menu, i);
        if (!option || option->disable || option->kind != OPTKIND_MENU || !option->menu) continue;
        int length = MC_Path(option->menu, target, path, depth + 1);
        if (length) return length;
    }
    return 0;
}
static int MC_Jump(GOBJ *gobj, EventMenu *target)
{
    MenuData *data = gobj->userdata;
    EventMenu *path[4];
    for (int tab = 0; tab < data->root_menu->tab_num; ++tab) {
        int length = MC_Path(data->root_menu->tabs[tab].menu, target, path, 0);
        if (!length) continue;
        MC_Tab(data, tab);
        data->selector = 0;
        for (int depth = 1; depth < length; ++depth) {
            for (int i = 0; i < EventMenu_OptionCount(data->curr_menu); ++i)
                if (EventMenu_GetOption(data->curr_menu, i)->kind == OPTKIND_MENU && EventMenu_GetOption(data->curr_menu, i)->menu == path[depth]) { MC_Select(data, i); break; }
            EventMenu_NextMenu(gobj, path[depth]);
        }
        MC_UpdateText(gobj);
        return 1;
    }
    return 0;
}
