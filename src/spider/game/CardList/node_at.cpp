#include "game_api.h"

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
