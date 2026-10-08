#include "game_api.h"

void GameWin::move_run(int src_pile, int src_card, int dst_pile, int record, int flip)
{
    int rec[5];
    RECT r;
    int last;
    int empty;
    HDC hdc;
    TripleTable *t;
    int prev;

    if (board->counts[dst_pile] - 1 > 0)
        last = board->counts[dst_pile] - 1;
    else
        last = 0;
    empty = ((CardColumn *)board)->slot_empty(dst_pile);
    if (record == 0 || (src_card > 0 && ((DealView *)this)->suit_of(src_pile, src_card - 1) == 0)) {
        ((ColumnOp *)this)->place_on_top(src_pile, src_card, (int)&r);
        InvalidateRect(hwnd, &r, TRUE);
    }
    ((PileTable *)board)->splice(src_pile, src_card, dst_pile, last);
    if (record != 0) {
        rec[0] = 0;
        rec[1] = src_pile;
        rec[2] = dst_pile;
        if (empty)
            rec[3] = 0;
        else
            rec[3] = last + 1;
        rec[4] = 0;
        if (((CardColumn *)board)->slot_empty(src_pile) == 0) {
            prev = src_card - 1;
            if (((DealView *)this)->suit_of(src_pile, prev) == 0) {
                rec[0] = 1;
                board->extras[src_pile]--;
                t = (TripleTable *)mode;
                t->set_suit(((PileTable *)board)->card_value(src_pile, prev), 1);
                ((ColumnOp *)this)->place_on_top(src_pile, prev, (int)&r);
                InvalidateRect(hwnd, &r, FALSE);
            }
        }
        push_undo(rec);
    } else if (flip != 0) {
        board->extras[dst_pile]++;
        t = (TripleTable *)mode;
        t->set_suit(((PileTable *)board)->card_value(dst_pile, last), 0);
    }
    ((ColumnOp *)this)->place_on_top(dst_pile, last, (int)&r);
    InvalidateRect(hwnd, &r, FALSE);
    if (((ColumnOp *)this)->fit_piles()) {
        ((ColumnOp *)this)->place_on_top(dst_pile, last, (int)&r);
        r.top = 0;
        r.bottom = ((LayoutWin *)this)->deal_top();
        InvalidateRect(hwnd, &r, TRUE);
    }
    in_play++;
    f5c = 0;
    add_score(-1);
    hdc = GetDC(hwnd);
    ((GameBoard *)this)->paint_board(hdc, 0, 0);
    ReleaseDC(hwnd, hdc);
}
