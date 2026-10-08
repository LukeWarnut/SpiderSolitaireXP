#include "game_api.h"

#pragma auto_inline(off)
CardNode *CardList::node_at(int index)
{
    int n = 0;
    CardNode *node = head;

    for (; n < index; n++) {
        if (node == 0) {
            break;
        }
        node = node->link;
    }
    return node;
}

void PileTable::splice(int src, int src_i, int dst, int dst_i)
{
    CardList *from;
    CardList *to;
    CardNode *node;
    CardNode *prev;
    CardNode *at;

    from = piles[src];
    to = piles[dst];
    if (from == 0) {
        return;
    }
    if (to == 0) {
        return;
    }
    node = from->node_at(src_i);
    if (node == 0) {
        return;
    }
    prev = node->prev;
    if (prev != 0) {
        prev->link = 0;
    } else {
        from->head = 0;
    }
    if (counts[dst] == 0) {
        to->head = node;
        node->prev = 0;
    } else {
        at = to->node_at(dst_i);
        if (at != 0) {
            at->link = node;
            node->prev = at;
        }
    }
    counts[dst] += counts[src] - src_i;
    counts[src] = src_i;
}
#pragma auto_inline(on)
