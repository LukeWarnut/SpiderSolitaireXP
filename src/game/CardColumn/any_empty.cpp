#include "game_api.h"

int CardColumn::any_empty()
{
    int i;
    for (i = 0; i < 10; i++) {
        if (slots[i] == 0) {
            return 1;
        }
    }
    return 0;
}
