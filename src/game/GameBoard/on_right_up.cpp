#include "game_api.h"

int GameBoard::on_right_up(HWND, UINT, WPARAM, LPARAM)
{
    int y;
    HDC hdc;
    int card;
    int x;
    int pile;

    if (f24 != 0) {
        ReleaseCapture();
        if (a < 0) {
            RECT drag;
            ((ColumnOp *)this)->place(drag_pile, drag_card, board->counts[drag_pile] - 1, (int)&drag);
            InvalidateRect(((LayoutBox *)this)->hwnd, &drag, FALSE);
        } else {
            RECT drag;
            RECT box;
            RECT area;
            RECT hit;
            card = drag_card;
            x = (a + 0x47) * drag_pile + b;
            hdc = GetDC(((LayoutBox *)this)->hwnd);
            for (; card < board->counts[drag_pile]; card++) {
                pile = drag_pile;
                y = ((Layout *)this)->score(pile, card);
                draw_card(hdc, x, y, ((DealView *)this)->card_code(drag_pile, card), 1, 1);
            }
            ((ColumnOp *)this)->place_on_top(drag_pile, drag_card, (int)&hit);
            ((LayoutWin *)this)->center_rect(&area);
            if (IntersectRect(&drag, &hit, &area))
                paint_board(hdc, 0, 0);
            ((LayoutBox *)this)->fill_rect(&box);
            if (IntersectRect(&drag, &hit, &box))
                ((LayoutBox *)this)->draw_stock(hdc, 0, 0);
            ((DealBox *)this)->fill(&box);
            if (IntersectRect(&drag, &hit, &box))
                ((GameWin *)this)->draw_cleared(hdc, 0, 0);
            ReleaseDC(((LayoutBox *)this)->hwnd, hdc);
        }
        f24 = 0;
    }
    return 0;
}
