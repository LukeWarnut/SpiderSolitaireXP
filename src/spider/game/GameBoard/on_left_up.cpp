#include "game_api.h"
#include <mmsystem.h>

int GameBoard::on_left_up(HWND hwnd, UINT, WPARAM, LPARAM lp)
{
    int x;
    int y;
    RECT box;
    RECT deck;
    RECT cr;
    RECT hit;
    POINT pt;
    int target;
    int i;
    int last;
    int done;

    x = LOWORD(lp);
    y = HIWORD(lp);
    if (f20 == 0) {
        if (suit_lock == 0) {
            if (f58 < 5) {
                ((LayoutBox *)this)->fill_rect(&box);
                if (x > box.left && x < box.right && y > box.top && y < box.bottom)
                    PostMessageW(hwnd, WM_COMMAND, 0x9c47, 0);
            }
            ((LayoutWin *)this)->center_rect(&deck);
            pt.x = x;
            pt.y = y;
            if (PtInRect(&deck, pt))
                PostMessageW(hwnd, WM_COMMAND, 0x9c4d, 0);
        }
    } else {
        ReleaseCapture();
        f20 = 0;
        if (((GameWin *)this)->play_snd != 0)
            PlaySoundW((LPCWSTR)0x7d, GetModuleHandleW(0), 0x40007);
        BitBlt((HDC)drag_a, drag_rc.left, drag_rc.top, drag_x1, drag_y1, (HDC)drag_b, 0, 0, SRCCOPY);
        target = -1;
        i = ((LayoutBox *)this)->pile_at(drag_rc.left);
        last = ((LayoutBox *)this)->pile_at(drag_rc.right);
        if (last < 0) {
            last = i + 1;
            if (last >= 9)
                last = 9;
        }
        for (; i <= last; i++) {
            if (target != -1)
                break;
            if (i >= 0 && i < 10) {
                ((ColumnOp *)this)->place_on_top(i, board->counts[i] - 1, (int)&cr);
                if (IntersectRect(&hit, &cr, &drag_rc) &&
                    ((DealView *)this)->can_drop(lift_pile, lift_card, i))
                    target = i;
            }
        }
        done = -1;
        if (target >= 0) {
            ((GameWin *)this)->move_run(lift_pile, lift_card, target, 1, 0);
            if (((DealView *)this)->full_suit(target))
                done = target;
        } else {
            InvalidateRect(((LayoutBox *)this)->hwnd, &drag_home, FALSE);
        }
        SelectObject((HDC)drag_b, old_a);
        SelectObject((HDC)drag_c, old_b);
        SelectObject((HDC)drag_d, old_c);
        DeleteObject(bmp_a);
        DeleteObject(bmp_b);
        DeleteObject(bmp_c);
        DeleteDC((HDC)drag_b);
        DeleteDC((HDC)drag_c);
        DeleteDC((HDC)drag_d);
        ReleaseDC(((LayoutBox *)this)->hwnd, (HDC)drag_a);
        if (done >= 0)
            ((GameWin *)this)->take_suit(done);
    }
    return 0;
}
