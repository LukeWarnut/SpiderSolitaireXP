#include "game_api.h"

int LayoutBox::pile_at(int x)
{
    int q;
    int r;
    if (x < b) {
        return -2;
    }
    q = (x - b) / (a + 0x47);
    r = (x - b) % (a + 0x47);
    if (r > 0x47) {
        return -2;
    }
    if (q < 10) {
        return q;
    }
    return -2;
}
