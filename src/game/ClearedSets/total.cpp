#include "game_api.h"

int ClearedSets::total()
{
    int sum = 0;
    int i;
    for (i = 0; i < 4; i++) {
        sum += vals[i];
    }
    return sum;
}
