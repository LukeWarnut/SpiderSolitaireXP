#include "game_api.h"

void TenListBoard::destroy_lists()
{
    int i;
    for (i = 0; i < 10; i++) {
        CardList *p = lists[i];
        if (p != 0) {
            p->release(1);
        }
        lists[i] = 0;
    }
}
