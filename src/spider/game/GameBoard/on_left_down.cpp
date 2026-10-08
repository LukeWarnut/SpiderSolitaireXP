#include "game_api.h"
#include <mmsystem.h>

int GameBoard::on_left_down(HWND, UINT, WPARAM, LPARAM lp)
{
    int x;
    int y;
    int pile;
    int card;
    int n;
    int k;
    int i;
    int s;

    x = LOWORD(lp);
    y = HIWORD(lp);
    if (f24 == 0 && suit_lock == 0) {
        pile = ((LayoutBox *)this)->pile_at(x);
        if (pile != -2) {
            card = ((ColumnOp *)this)->hit_card(pile, y);
            if (card != -2 && ((DealView *)this)->run_ok(pile, card)) {
                if (((GameWin *)this)->play_snd != 0)
                    PlaySoundW((LPCWSTR)0x80, GetModuleHandleW(0), 0x40007);
                SetCapture(((LayoutBox *)this)->hwnd);
                lift_card = card;
                lift_pile = pile;
                f20 = 1;
                ((ColumnOp *)this)->place_on_top(pile, card, (int)&drag_rc);
                drag_home = drag_rc;
                drag_x1 = drag_rc.right - drag_rc.left;
                drag_y1 = drag_rc.bottom - drag_rc.top;
                origin_x = x - drag_rc.left;
                origin_y = y - drag_rc.top;
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
                paint_column((HDC)drag_b, pile, card);
                n = board->counts[pile];
                i = card;
                s = 0;
                if (i < n) {
                    k = 0;
                    do {
                        s = extras[pile];
                        draw_card((HDC)drag_c, 0, k * s, ((DealView *)this)->card_code(pile, i), 1,
                                  i == n - 1);
                        i++;
                        k++;
                    } while (i < n);
                }
            }
        }
    }
    return 0;
}
