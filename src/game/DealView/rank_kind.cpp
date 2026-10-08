#include "game_api.h"

int DealView::rank_kind(int p0, int c0, int p1, int c1)
{
    int kind;

    kind = 2;
    if (rank_of(p0, c0) == rank_of(p1, c1)) {
        kind = 3;
    } else if (((CardColumn *)col)->slot_empty(p1)) {
        kind = 0;
        kind++;
    }
    return kind;
}
