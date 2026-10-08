#include "game_api.h"
#include <new>

int CardList::append(int *value, int *count)
{
    CardNode *node;
    CardNode *t;

    if (value != 0) {
        node = (CardNode *)operator new(0xc);
        if (node != 0) {
            t = tail(count);
            if (t != 0) {
                t->link = node;
            } else {
                head = node;
            }
            node->value = *value;
            node->link = 0;
            node->prev = t;
            if (count != 0) {
                if (t == 0) {
                    *count = (int)t;
                } else {
                    (*count)++;
                }
            }
            return 1;
        }
    }
    return 0;
}
