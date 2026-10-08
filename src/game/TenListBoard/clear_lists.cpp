#include "game_api.h"

void TenListBoard::clear_lists()
{
    int i;
    for (i = 0; i < 10; i++) {
        lists[i]->drain();
        counts[i] = 0;
        extras[i] = 0;
    }
}
