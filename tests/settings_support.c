/* Freestanding memory routines and observability for the PowerPC test image. */
#include "../src/events.h"
#include <stddef.h>

void memcpy(void *dst, const void *src, int size) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (size--) *d++ = *s++;
}
void memset(void *dst, int value, int size) {
    unsigned char *d = dst;
    while (size--) *d++ = value;
}
int TestDirty(void) { return stc_memcard_state->memcard_changed; }
void TestClearDirty(void) { stc_memcard_state->memcard_changed = 0; }
