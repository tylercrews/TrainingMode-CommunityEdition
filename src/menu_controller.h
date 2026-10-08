#ifndef TM_MENU_CONTROLLER_H
#define TM_MENU_CONTROLLER_H

/* Portable cursor rules shared by the native controller menu and its tests.
 * Index -1 is the tab/page selector; values are always changed one step. */
static inline int MenuController_Move(int index, int count, int selector, int direction)
{
    int first = selector ? -1 : 0;
    int span = count - first;
    if (span <= 0) return first;
    if (index < first || index >= count) index = first;
    return first + (index - first + (direction < 0 ? span - 1 : 1)) % span;
}

static inline int MenuController_Step(int value, int minimum, int count, int direction)
{
    if (count <= 0) return minimum;
    int maximum = minimum + count - 1;
    value += direction < 0 ? -1 : 1;
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

static inline int MenuController_Scroll(int first, int index, int count, int visible)
{
    int maximum = count > visible ? count - visible : 0;
    if (index >= 0) {
        if (index < first) first = index;
        if (index >= first + visible) first = index - visible + 1;
    }
    if (first < 0) first = 0;
    return first > maximum ? maximum : first;
}

#endif
