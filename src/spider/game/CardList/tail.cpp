#include "game_api.h"

CardNode *CardList::tail(int *count)
{
    int n = 0;
    CardNode *node;

    node = node_at(0);
    while (node != 0) {
        if (node->link == 0) {
            break;
        }
        node = node->link;
        n++;
    }
    if (count != 0) {
        *count = n;
    }
    return node;
}
