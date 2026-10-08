#include "game_api.h"

int CardColumn::slot_empty(int index)
{
    return slots[index] == 0;
}
