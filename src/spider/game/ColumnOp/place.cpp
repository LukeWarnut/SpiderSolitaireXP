#include "game_api.h"

void ColumnOp::place(int pile, int lo, int hi, int out)
{
    Quad4 *rc;
    int cap;

    rc = (Quad4 *)out;
    rc->a = (a + 0x47) * pile + b;
    rc->c = rc->a + 0x47;
    rc->b = ((Layout *)this)->score(pile, lo);
    cap = inner->caps[pile];
    if (((CardColumn *)inner)->slot_empty(pile)) {
        rc->d = rc->b + 0x60;
    } else if (lo < cap) {
        if (hi < cap) {
            rc->d = (hi - lo) * 7 + rc->b + 0x60;
        } else {
            rc->d = extras[pile] * (hi - cap) + cap * 7 + 0x6a;
        }
    } else {
        rc->d = extras[pile] * (hi - lo) + rc->b + 0x60;
    }
}
