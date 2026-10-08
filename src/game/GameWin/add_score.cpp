#include "game_api.h"

void GameWin::add_score(int delta)
{
    int *lim;
    int m;

    m = *mode;
    if (m == 1) {
        lim = &suit_slot[0][0];
    } else if (m == 2) {
        lim = &suit_slot[1][0];
    } else {
        lim = &suit_slot[2][0];
    }
    int *p = &suit_src;
    int s = *p;
    if (s + delta < 0) s = 0; else s += delta;
    *p = s;
    if (s > *lim) *lim = s;
}
