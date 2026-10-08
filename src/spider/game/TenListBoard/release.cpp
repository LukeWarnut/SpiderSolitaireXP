#include "game_api.h"

void *__thiscall TenListBoard::release(unsigned char flags)
{
    destroy_lists();
    if (flags & 1) {
        heap_free(this);
    }
    return this;
}
