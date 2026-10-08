#include "game_api.h"

int ColumnOp::fit_piles()
{
    RECT client;
    RECT rc;
    int result = 0;
    int pile;
    int top;
    int saved;

    GetClientRect(((LayoutWin *)this)->hwnd, &client);
    if (client.bottom <= 0x6a) {
        return 0;
    }
    for (pile = 0; pile < 10; pile++) {
        if (((CardColumn *)inner)->slot_empty(pile) == 0) {
            saved = extras[pile];
            extras[pile] = 0x1c;
            top = ((PileTable *)inner)->counts[pile] - 1;
            for (;;) {
                place_on_top(pile, top, (int)&rc);
                if (rc.bottom < ((LayoutWin *)this)->deal_top()) {
                    break;
                }
                if (extras[pile] < 0x10) {
                    if (rc.top < ((LayoutWin *)this)->deal_top() - 0x10) {
                        break;
                    }
                }
                if (extras[pile] < 1) {
                    break;
                }
                extras[pile]--;
            }
            if (extras[pile] != saved) {
                rc.top = 0;
                rc.bottom = client.bottom;
                InvalidateRect(((LayoutWin *)this)->hwnd, &rc, 1);
                result = 1;
            }
        }
    }
    return result;
}
