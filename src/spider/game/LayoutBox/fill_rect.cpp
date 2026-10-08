#include "draw_stock.cpp"

#pragma auto_inline(off)
void LayoutBox::fill_rect(RECT *out)
{
    RECT rc;
    int n;

    GetClientRect(hwnd, &rc);
    n = (inner->hi - inner->lo + 9) / 10 - 1;
    out->left = card_x(n);
    out->right = card_x(0) + 0x47;
    out->top = rc.bottom + (-0x6a);
    out->bottom = out->top + 0x60;
}
#pragma auto_inline(on)
