#include "game_api.h"

void AttrTable::clear_buf()
{
    void *block = p;
    if (block != 0) {
        heap_free(block);
        p = 0;
    }
}
