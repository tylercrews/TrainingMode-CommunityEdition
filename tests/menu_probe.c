/* Execute the actual PowerPC menu views and controller rules in Unicorn.
 * Gameplay callbacks are never invoked by this metadata/navigation probe. */
#include "../src/lab.h"
#include "../src/recovery.c"
#include "../src/lab_menu.h"
#include "../src/menu_controller.h"
#include "../src/menu_controller_layout.h"

int TestMenuInit(void) { Lab_InitMenuUI(); return 1; }
int TestMenuMove(int i, int n, int selector, int direction) { return MenuController_Move(i, n, selector, direction); }
int TestMenuStep(int value, int minimum, int count, int direction) { return MenuController_Step(value, minimum, count, direction); }
int TestMenuScroll(int first, int index, int count, int visible) { return MenuController_Scroll(first, index, count, visible); }
int TestMenuPreviewStart(int lines) { return MenuController_PreviewStart(lines); }
int TestMenuPreviewCapacity(int first) { return MenuController_PreviewCapacity(first); }
int TestMenuPreviewBottom(void) { return MC_PREVIEW_BOTTOM; }
int TestMenuPreviewStep(void) { return MC_PREVIEW_STEP; }
int TestMenuDescriptionStep(void) { return MC_DESC_STEP; }
int TestMenuTabCount(void) { return LabMenu_Main.tab_num; }
unsigned TestMenuTab(int tab) { return (unsigned)LabMenuTabs[tab].menu; }
int TestMenuCount(EventMenu *menu) { return EventMenu_OptionCount(menu); }
int TestMenuPages(EventMenu *menu) { return menu->page_num; }
int TestMenuPage(EventMenu *menu, int page) { menu->page = page; return page; }
unsigned TestMenuOption(EventMenu *menu, int row) { return (unsigned)EventMenu_GetOption(menu, row); }
unsigned TestMenuTarget(EventOption *option) { return option->kind == OPTKIND_MENU ? (unsigned)option->menu : 0; }
unsigned TestMenuName(EventMenu *menu) { return (unsigned)menu->name; }
int TestMenuOrigin(EventOption *option)
{
    static EventOption *const arrays[] = {LabOptions_General, LabOptions_CPU, LabOptions_Record, LabOptions_Tech, LabOptions_OSDs,
        LabOptions_Controls, LabOptions_OverlaysHMN, LabOptions_OverlaysCPU, LabOptions_InfoDisplayHMN, LabOptions_InfoDisplayCPU,
        LabOptions_AlterInputs, LabOptions_Main};
    static const int counts[] = {OPTGEN_COUNT, OPTCPU_COUNT, OPTREC_COUNT, OPTTECH_COUNT, countof(LabOptions_OSDs),
        OPTCTRL_COUNT, OVERLAY_COUNT, OVERLAY_COUNT, OPTINF_COUNT, OPTINF_COUNT, OPTINPUT_COUNT, OPTLAB_COUNT};
    for (int a = 0; a < (int)countof(arrays); ++a)
        for (int i = 0; i < counts[a]; ++i) if (option == &arrays[a][i]) return (a + 1) * 100 + i;
    return 0;
}
int TestMenuValue(EventOption *option) { return option->val; }
int TestMenuWriteSource(int array, int index, int value)
{
    if (array == 1) LabOptions_General[index].val = value;
    if (array == 2) LabOptions_CPU[index].val = value;
    if (array == 5) LabOptions_OSDs[index].val = value;
    return value;
}
