#include "game_api.h"

int *TripleTable::next()
{
    int i = idx;
    if (i >= limit) {
        return 0;
    }
    idx = i + 1;
    return &ptr[i * 3];
}
