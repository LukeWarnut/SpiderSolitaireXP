#include "game_api.h"

int ColumnOp::hit_card(int pile, int y)
{
    int n;
    int cap;
    int step;
    int top;
    int mid;
    int r;

    if (y < 0xa) {
        return -2;
    }
    y += -0xa;
    if (((CardColumn *)inner)->slot_empty(pile)) {
        if (y < 0x60) {
            return -1;
        }
    }
    n = inner->slots[pile];
    cap = inner->caps[pile];
    step = extras[pile];
    top = cap * 7;
    mid = (n - cap - 1) * step;
    r = -2;
    if (y < top) {
        r = y / 7;
    } else if (y < top + mid) {
        r = (y - top) / step + cap;
    } else if (y < top + mid + 0x60) {
        r = n - 1;
    }
    return r;
}
