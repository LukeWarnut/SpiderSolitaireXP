#include "game_api.h"

void LayoutWin::center_rect(RECT *rc)
{
    int w;

    GetClientRect(hwnd, rc);
    w = rc->right - rc->left;
    rc->left = (w - 0xc8) / 2;
    rc->right = (w + 0xc8) / 2;
    rc->top = deal_top();
    rc->bottom += -10;
}
