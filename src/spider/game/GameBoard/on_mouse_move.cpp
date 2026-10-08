#include "game_api.h"

int GameBoard::on_mouse_move(HWND, UINT, WPARAM wp, LPARAM lp)
{
    RECT old;
    int x;
    int y;
    unsigned packed;

    if ((wp & 1) == 0) {
        return 0;
    }
    if (f20 == 0) {
        return 0;
    }
    packed = (unsigned)lp;
    old = drag_rc;
    x = (short)packed;
    x -= origin_x;
    y = (short)(packed >> 16);
    y -= origin_y;
    drag_rc.left = x;
    drag_rc.top = y;
    drag_rc.right = drag_x1 + x;
    drag_rc.bottom = drag_y1 + y;
    refresh_drag(drag_a, drag_d, drag_b, drag_c, &old, &drag_rc);
    return 0;
}
