#include "game_api.h"

void ColumnOp::place_on_top(int a, int b, int c)
{
    place(a, b, inner->slots[a] - 1, c);
}
