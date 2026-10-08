#include "game_api.h"

int GameBoard::on_right_down(HWND, UINT, WPARAM, LPARAM lp)
{
    unsigned packed;
    int pile;
    int card;
    int aa;
    int bb;
    int y;
    HDC hdc;

    if (f20 != 0) {
        return 0;
    }
    packed = (unsigned)lp;
    pile = ((LayoutBox *)this)->pile_at((unsigned short)packed);
    if (pile == -2) {
        return 0;
    }
    packed >>= 16;
    card = ((ColumnOp *)this)->hit_card(pile, packed);
    if (card == -2) {
        return 0;
    }
    if (card < ((CardColumn *)board)->caps[pile]) {
        return 0;
    }
    f24 = 1;
    drag_pile = pile;
    drag_card = card;
    SetCapture(((LayoutBox *)this)->hwnd);
    aa = a;
    bb = b;
    y = ((Layout *)this)->score(pile, card);
    hdc = GetDC(((LayoutBox *)this)->hwnd);
    draw_card(
        hdc,
        (aa + 0x47) * pile + bb,
        y,
        ((DealView *)this)->card_code(pile, card),
        1,
        1);
    ReleaseDC(((LayoutBox *)this)->hwnd, hdc);
    return 0;
}
