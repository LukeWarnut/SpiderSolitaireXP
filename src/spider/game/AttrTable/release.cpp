#include "game_api.h"

void *__thiscall AttrTable::release(unsigned char flags)
{
    clear_buf();
    if (flags & 1) {
        heap_free(this);
    }
    return this;
}
