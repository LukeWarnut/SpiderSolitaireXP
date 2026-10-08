#include "game_api.h"

void GameWin::anim_deal(int pile, int card)
{
    GameBoard *g = (GameBoard *)this;
    TripleTable *m;
    RECT to;
    RECT client;
    RECT dirty;
    RECT from;
    int deals;
    int left;

    m = (TripleTable *)mode;
    g->drag_x1 = 0x47;
    g->drag_y1 = 0x60;
    GetClientRect(hwnd, &client);
    left = m->limit - m->idx + 1;
    deals = (left + 9) / 10;
    from.left = ((LayoutBox *)this)->card_x(deals - 1);
    from.top = client.bottom - 0x6a;
    from.right = g->drag_x1 + from.left;
    from.bottom = g->drag_y1 + from.top;
    dirty = from;
    ((ColumnOp *)this)->place(pile, card, card, (int)&to);
    g->drag_a = (int)GetDC(hwnd);
    g->drag_b = (int)CreateCompatibleDC((HDC)g->drag_a);
    g->drag_c = (int)CreateCompatibleDC((HDC)g->drag_a);
    g->drag_d = (int)CreateCompatibleDC((HDC)g->drag_a);
    g->bmp_a = CreateCompatibleBitmap((HDC)g->drag_a, g->drag_x1, g->drag_y1);
    g->bmp_b = CreateCompatibleBitmap((HDC)g->drag_a, g->drag_x1, g->drag_y1);
    g->bmp_c = CreateCompatibleBitmap((HDC)g->drag_a, g->drag_x1, g->drag_y1);
    g->old_a = SelectObject((HDC)g->drag_b, g->bmp_a);
    g->old_b = SelectObject((HDC)g->drag_c, g->bmp_b);
    g->old_c = SelectObject((HDC)g->drag_d, g->bmp_c);
    g->draw_card((HDC)g->drag_c, 0, 0, ((DealView *)this)->card_code(pile, card), 1, 1);
    if (left % 10 == 1) {
        g->draw_felt((HDC)g->drag_b, 0, 0, &dirty, &dirty);
        if (deals > 1)
            g->draw_card((HDC)g->drag_b, 0xc, 0, 0x68, 1, 1);
    } else {
        g->draw_card((HDC)g->drag_b, 0, 0, 0x68, 1, 1);
    }
    g->slide_drag((HDC)g->drag_a, (HDC)g->drag_c, (HDC)g->drag_b, (HDC)g->drag_d, &from, &to, 0x32);
    SelectObject((HDC)g->drag_b, g->old_a);
    SelectObject((HDC)g->drag_c, g->old_b);
    SelectObject((HDC)g->drag_d, g->old_c);
    DeleteObject(g->bmp_a);
    DeleteObject(g->bmp_b);
    DeleteObject(g->bmp_c);
    DeleteDC((HDC)g->drag_b);
    DeleteDC((HDC)g->drag_c);
    DeleteDC((HDC)g->drag_d);
    ReleaseDC(hwnd, (HDC)g->drag_a);
    InvalidateRect(hwnd, &dirty, FALSE);
}
