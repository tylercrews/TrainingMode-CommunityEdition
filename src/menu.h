#include "../MexTK/mex.h"
#include <stdint.h>

#define MENU_MAXOPTION 9
#define MENU_DESCLINEMAX 4
#define MENU_SCROLLOFF 2
#define MENU_RAPID_INTERVAL 5
#define MENU_RAPID_START 15
#define MENU_RAPID_R 2

// Custom File Structs
typedef struct evMenu
{
    JOBJDesc *menu;
    JOBJDesc *popup;
    JOBJDesc *scroll;
    JOBJDesc *check;
    JOBJDesc *arrow;
    JOBJDesc *playback;
    JOBJDesc *message;
    COBJDesc *hud_cobjdesc;
    JOBJDesc *tip_jobj;
    void **tip_jointanim; // pointer to array
} evMenu;

// Structure Definitions
typedef struct EventMenu EventMenu;
typedef struct EventOption
{
    u8 kind;                                        // the type of option this is; string, integers, etc
    u8 disable;                                     // boolean for disabling the option
    s16 value_min;                                  // minimum value
    u16 value_num;                                  // number of values
    s16 val;                                        // value of this option
    s16 val_prev;                                   // previous value of this option
    char *name;                                     // pointer to the name of this option
    char *desc[MENU_DESCLINEMAX];                   // pointer to the description string array for this option
    union {
        EventMenu *menu;                            // pointer to submenu for OPTKIND_MENU
        const char **values;                        // options for OPTKIND_STRING
        const char *format;                         // format string for OPTKIND_INT
        const char *value_string;                   // optional value string for OPTKIND_FUNC / OPTKIND_INFO 
    };
    void (*OnChange)(GOBJ *menu_gobj, int value);   // function that runs when option is changed
    void (*OnSelect)(GOBJ *menu_gobj);              // function that runs when option is selected
} EventOption;
/* Presentation rows reference the canonical option, including runtime label,
 * value and availability changes. Existing contiguous menus remain supported. */
typedef struct EventMenuRow {
    EventOption *option;
    u8 group;
} EventMenuRow;
typedef struct EventMenuPage {
    char *name;
    u8 option_num;
    EventMenuRow *rows;
} EventMenuPage;
typedef struct EventMenuTab {
    char *name;
    EventMenu *menu;
} EventMenuTab;
typedef struct Shortcut {
    int button_mask;
    EventOption *option;
} Shortcut;
typedef struct ShortcutList {
    int count;
    Shortcut *list;
} ShortcutList;
struct EventMenu
{
    char *name;                    // name of this menu
    u8 option_num;                 // number of options this menu contains
    u8 scroll;                     //
    u8 cursor;                     // index of the option currently selected
    EventOption *options;          // pointer to all of this menu's options
    EventMenu *prev;               // pointer to previous menu, used at runtime
    ShortcutList *shortcuts;       // pointer to shortcuts when shortcut mode is entered on this menu
    EventMenuRow *rows;            // optional referenced presentation rows
    EventMenuPage *pages;          // optional peer pages within this editor
    u8 page_num;
    u8 page;
    EventMenuTab *tabs;            // root-only: opt in to the controller renderer
    u8 tab_num;
    char *purpose;
    const char *(*unavailable_reason)(EventOption *option); /* optional root context */
};
typedef enum MenuMode {
    MenuMode_Normal,
    MenuMode_Paused,
} MenuMode;
typedef struct MenuData
{
    EventMenu *curr_menu;
    u16 canvas_menu;
    u16 canvas_popup;
    u8 hide_menu;                                                                            // enable this to hide the base menu. used for custom menus.
    u8 mode;
    u8 controller_index; // index of the controller who paused
    Text *text_name;
    Text *text_value;
    Text *text_title;
    Text *text_desc;
    JOBJ *rowboxes[MENU_MAXOPTION];
    JOBJ *highlight_menu;                // pointer to the highlight jobj
    JOBJ *scrollbar;
    JOBJ *scroll_top;
    JOBJ *scroll_bot;
    GOBJ *custom_gobj;                               // onSelect gobj
    int (*custom_gobj_think)(GOBJ *custom_gobj);     // per frame function. Returns bool indicating if the program should check to unpause
    void (*custom_gobj_destroy)(GOBJ *custom_gobj);  // on destroy function
    EventMenu *root_menu;
    u8 tab;
    u8 selector;                   // focus on the category tabs / editor page
    Text *text_tabs;
    Text *text_preview;
    Text *text_hints;
    JOBJ *tabboxes[8];
    EventOption *picker;
    s16 picker_value;              // draft; original option is untouched until A
    u16 picker_scroll;
    Text *text_panel_title;
    EventMenu *preview_menu;
    u8 preview_page;
    u8 preview_pages;
} MenuData;

static inline int EventMenu_OptionCount(EventMenu *menu) {
    return menu->page_num ? menu->pages[menu->page].option_num : menu->option_num;
}
static inline EventOption *EventMenu_GetOption(EventMenu *menu, int index) {
    if (index < 0 || index >= EventMenu_OptionCount(menu)) return 0;
    if (menu->page_num) return menu->pages[menu->page].rows[index].option;
    return menu->rows ? menu->rows[index].option : &menu->options[index];
}
static inline EventOption *EventMenu_SelectedOption(MenuData *data) {
    return EventMenu_GetOption(data->curr_menu, data->curr_menu->scroll + data->curr_menu->cursor);
}
static inline int EventMenu_OptionGroup(EventMenu *menu, int index) {
    if (menu->page_num) return menu->pages[menu->page].rows[index].group;
    return menu->rows ? menu->rows[index].group : 0;
}


GOBJ *EventMenu_Init(EventMenu *start_menu);
void EventMenu_Draw(GOBJ *eventMenu);
void EventMenu_Update(GOBJ *gobj);
void EventMenu_ExitMenu(GOBJ *gobj);
void EventMenu_PrevMenu(GOBJ *gobj);
void EventMenu_NextMenu(GOBJ *gobj, EventMenu *menu);
void EventMenu_ChangeOptionVal(GOBJ *gobj, EventOption *option, int value);
void EventMenu_UpdateText(GOBJ *gobj);
void EventMenu_CreateText(GOBJ *gobj);
void EventMenu_CreateModel(GOBJ *gobj);
void EventMenu_MenuThink(GOBJ *gobj, EventMenu *currMenu);
void EventMenu_RunFuncOption(GOBJ *gobj, EventOption *func_option);
void EventMenu_TextGX(GOBJ *gobj, int pass);
void EventMenu_MenuGX(GOBJ *gobj, int pass);
void EventMenu_Update(GOBJ *gobj);

// EventOption kind definitions
enum option_kind {
    OPTKIND_MENU,
    OPTKIND_STRING,
    OPTKIND_INT,
    OPTKIND_FUNC,
    OPTKIND_INFO,
    OPTKIND_TOGGLE,
    OPTKIND_RESUME,
};

// GX Link args
#define GXLINK_MENUMODEL 12
#define GXPRI_MENUMODEL 80
#define GXLINK_MENUTEXT 12
#define GXPRI_MENUTEXT GXPRI_MENUMODEL + 1
// popup menu
#define GXPRI_POPUPMODEL GXPRI_MENUTEXT + 1
#define GXLINK_POPUPMODEL 12
#define GXPRI_POPUPTEXT GXPRI_POPUPMODEL + 1
#define GXLINK_POPUPTEXT 12
// cobj
#define MENUCAM_COBJGXLINK (1 << GXLINK_MENUMODEL) | (1 << GXLINK_MENUTEXT) | (1 << GXLINK_POPUPMODEL) | (1 << GXLINK_POPUPTEXT)
#define MENUCAM_GXPRI 9

// menu text object
#define MENU_CANVASSCALE 0.05
// menu title
#define MENU_TITLEXPOS -430
#define MENU_TITLEYPOS -366
#define MENU_TITLESCALE 2.3
#define MENU_TITLEASPECT 870
// menu description
#define MENU_DESCXPOS -21.5
#define MENU_DESCYPOS 12
#define MENU_DESCTXTASPECT 885
#define MENU_DESCYOFFSET 30
// menu option name
#define MENU_OPTIONNAMEXPOS -430
#define MENU_OPTIONNAMEYPOS -230
#define MENU_NAMEASPECT 440
// menu option value
#define MENU_OPTIONVALXPOS 250
#define MENU_OPTIONVALYPOS -230
#define MENU_TEXTYOFFSET 50
#define MENU_VALASPECT 280
// menu scroll
#define MENUSCROLL_SCALE 2
#define MENUSCROLL_SCALEY 1.105 * MENUSCROLL_SCALE
#define MENUSCROLL_X 22.5
#define MENUSCROLL_Y 12
#define MENUSCROLL_MAXLENGTH 10
#define MENUSCROLL_COLOR { 255, 211, 0, 255 }
// row jobj
#define ROWBOX_HEIGHT 2.3
#define ROWBOX_WIDTH 18.f
#define ROWBOX_X 12.5
#define ROWBOX_Y 10.8
#define ROWBOX_YOFFSET -2.5
#define ROWBOX_COLOR { 25, 25, 45, 255 }
#define ROWBOX_ONCOLOR { 25, 225, 25, 255 }
// menu highlight
#define MENUHIGHLIGHT_HEIGHT ROWBOX_HEIGHT
#define MENUHIGHLIGHT_WIDTH 43.175
#define MENUHIGHLIGHT_X 0.f
#define MENUHIGHLIGHT_Y ROWBOX_Y
#define MENUHIGHLIGHT_YOFFSET ROWBOX_YOFFSET
#define MENUHIGHLIGHT_COLOR { 255, 211, 0, 255 }
