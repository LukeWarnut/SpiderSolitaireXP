#include "game_api.h"

void GameWin::draw_cleared(void *hdc, int dx, int dy)
{
    int n;
    int top;
    int x_base;
    int dy_run;
    int i;
    int *slot;
    int h;
    int y;

    n = ((ClearedSets *)this)->total();
    top = ((LayoutWin *)this)->deal_top();
    if (n <= 0) {
        return;
    }
    x_base = top + dy;
    dy_run = 0;
    slot = (int *)((char *)this + 0xF20);
    i = n;
    do {
        h = (*slot + 1) * 0xd;
        y = *(int *)((char *)this + 0x18) + dy_run + dx;
        ((GameBoard *)this)->draw_card(hdc, y, x_base, h, 1, 1);
        dy_run += 0xc;
        slot++;
        i--;
    } while (i != 0);
}
