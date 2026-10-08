#include "game_api.h"

int TripleTable::suit(int i)
{
    return ptr[i * 3 + 2];
}
