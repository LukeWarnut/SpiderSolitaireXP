#include "game_api.h"

void GameBoard::paint_column(HDC hdc, int pile, int card)
{
    RECT client;
    RECT r;
    int dx;
    int hidden;
    int h;
    int s;
    int off;
    int x;
    int n;
    int i;
    int saved;
    int x2;
    int n2;
    int h2;
    int s2;
    int ox;
    int oy;

    ((ColumnOp *)this)->place_on_top(pile, card, (int)&r);
    GetClientRect(((LayoutBox *)this)->hwnd, &client);
    draw_felt(hdc, 0, 0, &client, &r);
    hidden = board->extras[pile];
    dx = a;
    if (dx < 0) {
        if (card == 0)
            off = 0;
        else
            off = extras[pile] * (card - hidden) + hidden * 7;
        if (pile > 0) {
            x = -0x47 - dx;
            n = board->counts[pile - 1];
            h = board->extras[pile - 1];
            if (((CardColumn *)board)->slot_empty(pile - 1)) {
                draw_card(hdc, x, -off, 0x6c, 1, 1);
            } else {
                for (i = 0; i < h; i++)
                    draw_card(hdc, x, i * 7 - off, ((DealView *)this)->card_code(pile - 1, i), 1, 1);
                for (; i < n; i++) {
                    s = extras[pile - 1];
                    draw_card(hdc, x, (i - h) * s - off + h * 7,
                              ((DealView *)this)->card_code(pile - 1, i), 1, 1);
                }
            }
        }
    }
    if (card == 0) {
        if (dx < 0 && pile > 0) {
            saved = SaveDC(hdc);
            ExcludeClipRect(hdc, 0, 0, -dx, 0x60);
            draw_card(hdc, 0, 0, 0x6c, 1, 1);
            RestoreDC(hdc, saved);
        } else {
            draw_card(hdc, 0, 0, 0x6c, 1, 1);
        }
    } else if (((DealView *)this)->suit_of(pile, card - 1)) {
        hidden = extras[pile];
        draw_card(hdc, 0, -hidden, ((DealView *)this)->card_code(pile, card - 1), 0, 1);
    } else {
        draw_card(hdc, 0, -7, ((DealView *)this)->card_code(pile, card - 1), 0, 1);
    }
    if (dx < 0 && pile < 9) {
        x2 = dx + 0x47;
        if (((CardColumn *)board)->slot_empty(pile + 1)) {
            saved = SaveDC(hdc);
            ExcludeClipRect(hdc, x2, 0, 0x47, 0x60);
            draw_card(hdc, x2, -off, 0x6c, 1, 1);
            RestoreDC(hdc, saved);
        } else {
            n2 = board->counts[pile + 1];
            h2 = board->extras[pile + 1];
            for (i = 0; i < h2; i++)
                draw_card(hdc, x2, i * 7 - off, ((DealView *)this)->card_code(pile + 1, i), 1, 1);
            for (; i < n2; i++) {
                s2 = extras[pile + 1];
                draw_card(hdc, x2, (i - h2) * s2 - off + h2 * 7,
                          ((DealView *)this)->card_code(pile + 1, i), 1, 1);
            }
        }
    }
    oy = -r.top;
    ox = -r.left;
    paint_board(hdc, ox, oy);
    ((LayoutBox *)this)->draw_stock(hdc, ox, oy);
    ((GameWin *)this)->draw_cleared(hdc, ox, oy);
}
