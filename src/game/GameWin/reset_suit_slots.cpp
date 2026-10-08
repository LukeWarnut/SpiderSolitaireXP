#include "game_api.h"

void GameWin::reset_suit_slots()
{
    int n;
    int z;
    int *p;
    int m;

    p = &suit_slot[0][2];
    n = 3;
    z = 0;
    do {
        p[-2] = z;
        p[-1] = z;
        p[0] = z;
        p[1] = z;
        p[2] = z;
        p[3] = z;
        p[4] = z;
        p += 7;
        n--;
    } while (n != 0);
    if (suit_lock == 0) {
        m = *mode;
        if (m == 1) {
            suit_slot[0][0] = suit_src;
        } else if (m == 2) {
            suit_slot[1][0] = suit_src;
        } else {
            suit_slot[2][0] = suit_src;
        }
    }
}
