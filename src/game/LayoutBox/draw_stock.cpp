#include "game_api.h"

#pragma auto_inline(off)
int LayoutBox::card_x(int i)
{
    return (a + 0x47) * 9 - i * 12 + b;
}

void LayoutBox::draw_stock(void *hdc, int dx, int dy)
{
    int top;
    int span;
    int n;
    int i;
    LayoutInner *in;

    in = inner;
    top = ((LayoutWin *)this)->deal_top();
    span = in->hi - in->lo;
    if (span == 0x68) {
        return;
    }
    n = (span + 9) / 10;
    for (i = 0; i < n; i++) {
        ((GameBoard *)this)->draw_card(hdc, card_x(i) + dx, top + dy, 0x68, 1, 1);
    }
}
#pragma auto_inline(on)
