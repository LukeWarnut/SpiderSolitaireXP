#include "game_api.h"

void *__thiscall CardList::release(unsigned char flags)
{
    drain();
    if (flags & 1) {
        heap_free(this);
    }
    return this;
}
