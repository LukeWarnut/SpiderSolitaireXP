#include "game_api.h"

void DealBox::fill(RECT *out)
{
    int top;

    out->left = field18;
    top = ((LayoutWin *)this)->deal_top();
    out->top = top;
    out->right = ((ClearedSets *)this)->total() * 0x53 + out->left;
    out->bottom = top + 0x60;
}
