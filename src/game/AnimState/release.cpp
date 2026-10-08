#include "game_api.h"

void *__thiscall AnimState::release(unsigned char flags)
{
    stop();
    if (flags & 1) {
        heap_free(this);
    }
    return this;
}
