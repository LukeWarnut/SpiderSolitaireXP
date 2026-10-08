#include "game_api.h"

void GameBoard::begin_drag(int pile, int card)
{
    RECT from;
    RECT to;

    drag_x1 = 0x47;
    drag_y1 = 0x60;
    to.left = b - 0xc + ((ClearedSets *)this)->total() * 0xc;
    to.right = to.left + 0x47;
    to.top = ((LayoutWin *)this)->deal_top();
    to.bottom = drag_y1 + to.top;
    ((ColumnOp *)this)->place(pile, card, card, (int)&from);
    
    drag_a = (int)GetDC(((LayoutBox *)this)->hwnd);
    drag_b = (int)CreateCompatibleDC((HDC)drag_a);
    drag_c = (int)CreateCompatibleDC((HDC)drag_a);
    drag_d = (int)CreateCompatibleDC((HDC)drag_a);
    bmp_a = CreateCompatibleBitmap((HDC)drag_a, drag_x1, drag_y1);
    bmp_b = CreateCompatibleBitmap((HDC)drag_a, drag_x1, drag_y1);
    bmp_c = CreateCompatibleBitmap((HDC)drag_a, drag_x1, drag_y1);
    old_a = SelectObject((HDC)drag_b, bmp_a);
    old_b = SelectObject((HDC)drag_c, bmp_b);
    old_c = SelectObject((HDC)drag_d, bmp_c);

    draw_card((HDC)drag_c, 0, 0, ((DealView *)this)->card_code(pile, card), 1, 1);
    paint_column((HDC)drag_b, pile, card);
    slide_drag((HDC)drag_a, (HDC)drag_c, (HDC)drag_b, (HDC)drag_d, &from, &to, 0x32);
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
}
