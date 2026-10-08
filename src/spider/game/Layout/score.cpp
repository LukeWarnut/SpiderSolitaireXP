#include "game_api.h"

int Layout::score(int pile, int n)
{
    int cap;
    int r;
    if (n == -1) {
        return 0xa;
    }
    cap = inner->caps[pile];
    if (n < cap) {
        r = n * 7 + 0xa;
    } else {
        r = extras[pile] * (n - cap) + cap * 7 + 0xa;
    }
    return r;
}
