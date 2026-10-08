#include "game_api.h"

int LayoutWin::deal_top()
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    return rc.bottom + (-0x6a);
}
