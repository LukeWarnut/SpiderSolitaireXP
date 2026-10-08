#include "game_api.h"
#include <new>

TenListBoard::TenListBoard()
{
    int i;
    for (i = 0; i < 10; i++) {
        CardList *p = (CardList *)operator new(4);
        if (p != 0) {
            p->head = 0;
        } else {
            p = 0;
        }
        lists[i] = p;
        counts[i] = 0;
        extras[i] = 0;
    }
}
