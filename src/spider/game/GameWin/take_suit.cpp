#include "game_api.h"
#include <mmsystem.h>

void GameWin::take_suit(int pile)
{
    RECT box;
    RECT r;
    int n;
    int rank;
    int c;
    int top;
    int t;
    HDC hdc;
    TripleTable *tt;

    n = board->counts[pile];
    UpdateWindow(hwnd);
    rank = ((DealView *)this)->rank_of(pile, n - 1);
    z2[((ClearedSets *)this)->total()] = rank;
    z[rank]++;
    if (use_fx != 0) {
        for (c = n - 1; c >= n - 13; c--) {
            ((GameBoard *)this)->begin_drag(pile, c);
            ((PileTable *)board)->trim_pile(pile, c);
            if (play_snd != 0)
                PlaySoundW((LPCWSTR)0x7c, GetModuleHandleW(0), 0x40007);
        }
    } else {
        ((ColumnOp *)this)->place_on_top(pile, n - 13, (int)&r);
        InvalidateRect(hwnd, &r, FALSE);
        ((PileTable *)board)->trim_pile(pile, n - 13);
        if (play_snd != 0)
            PlaySoundW((LPCWSTR)0x7c, GetModuleHandleW(0), 0x40007);
    }
    level -= 13;
    disable_undo_menu();
    add_score(100);
    ((ColumnOp *)this)->fit_piles();
    if (use_fx == 0) {
        hdc = GetDC(hwnd);
        top = ((LayoutWin *)this)->deal_top();
        t = ((ClearedSets *)this)->total();
        ((GameBoard *)this)->draw_card(hdc, b - 0xc + t * 0xc, top, (z2[t - 1] + 1) * 0xd, 1, 1);
        ReleaseDC(hwnd, hdc);
    }
    if (((CardColumn *)board)->slot_empty(pile) == 0) {
        n = board->counts[pile];
        if (((DealView *)this)->suit_of(pile, n - 1) == 0) {
            board->extras[pile]--;
            tt = (TripleTable *)mode;
            tt->set_suit(((PileTable *)board)->card_value(pile, n - 1), 1);
            ((ColumnOp *)this)->place_on_top(pile, n - 1, (int)&r);
            InvalidateRect(hwnd, &r, FALSE);
        }
    }
    ((LayoutWin *)this)->center_rect(&box);
    InvalidateRect(hwnd, &box, FALSE);
    UpdateWindow(hwnd);
    if (level <= 0)
        won_game();
}
