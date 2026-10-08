#include "game_api.h"

int PileTable::card_value(int pile, int index)
{
    CardList *list = piles[pile];
    CardNode *node;

    if (list != 0) {
        node = list->node_at(index);
        if (node != 0) {
            return node->value;
        }
    }
    return -1;
}
