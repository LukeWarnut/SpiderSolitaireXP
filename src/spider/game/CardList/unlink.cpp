#include "game_api.h"

#pragma auto_inline(off)
int CardList::unlink(int index)
{
    CardNode *node;
    CardNode *next;
    CardNode *prev;

    node = node_at(index);
    if (node == 0) {
        return 0;
    }
    next = node->link;
    prev = node->prev;
    if (index == 0) {
        head = next;
        if (next != 0) {
            next->prev = 0;
        }
    } else {
        prev->link = next;
        if (next != 0) {
            next->prev = prev;
        }
    }
    heap_free(node);
    return 1;
}
#pragma auto_inline(on)

#include "../PileTable/splice.cpp"
