#include "game_api.h"

void TripleTable::set_suit(int i, int value)
{
    ptr[i * 3 + 2] = value;
}
