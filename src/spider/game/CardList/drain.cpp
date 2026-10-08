#include "game_api.h"

void CardList::drain()
{
    while (node_at(0) != 0) {
        unlink(0);
    }
    head = 0;
}
