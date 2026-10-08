#include "game_api.h"

void GameWin::load_game()
{
    RECT r;
    DWORD got;
    int count;
    int hidden;
    DWORD size;
    int value;
    int up;
    HANDLE h;
    int pile;
    int k;
    int *buf;
    int i;
    HMENU menu;

    i = 0;
    if (saved != 0) {
        if (suit_lock == 0 && opt_f70 != 0) {
            HWND w = hwnd;
            if (alert_box(w, 0x11, 2, 0x124) != IDYES)
                return;
        }
        h = open_saved(GENERIC_READ, OPEN_EXISTING);
        if (h != INVALID_HANDLE_VALUE) {
            size = GetFileSize(h, 0);
            buf = (int *)GlobalAlloc(0, size);
            if (buf != 0 && ReadFile(h, buf, size, &got, 0)) {
                if (buf[0] < 5) {
                    ((TripleTable *)mode)->mode = buf[0];
                    i = 1;
                } else {
                    ((TripleTable *)mode)->mode = 4;
                }
                seed = buf[i++];
                ((TripleTable *)mode)->shuffle(seed);
                ((TripleTable *)mode)->idx = buf[i++];
                level = buf[i++];
                in_play = buf[i++];
                f58 = buf[i++];
                for (k = 0; k < 4; k++)
                    z[k] = buf[i++];
                if (buf[i] < 5) {
                    for (k = 0; k < 8; k++)
                        z2[k] = buf[i++];
                }
                board->clear_lists();
                for (pile = 0; pile < 10; pile++) {
                    count = buf[i++];
                    hidden = buf[i++];
                    for (k = 0; k < count; k++) {
                        value = buf[i++];
                        up = k >= hidden;
                        board->add_card(pile, value, up);
                        ((TripleTable *)mode)->set_suit(value, up);
                    }
                }
                if ((DWORD)(i * 4) < size)
                    suit_src = buf[i];
                else
                    suit_src = 500;
                GlobalFree(buf);
                CloseHandle(h);
                f5c = 0;
                disable_undo_menu();
                menu = GetMenu(hwnd);
                EnableMenuItem(menu, 0x9c4b, MF_ENABLED);
                EnableMenuItem(menu, 0x9c46, MF_ENABLED);
                EnableMenuItem(menu, 0x9c4d, MF_ENABLED);
                if (f58 < 5) {
                    EnableMenuItem(menu, 0x9c47, MF_ENABLED);
                    EnableMenuItem(menu, 0x9c50, MF_ENABLED);
                } else {
                    EnableMenuItem(menu, 0x9c47, MF_GRAYED);
                    EnableMenuItem(menu, 0x9c50, MF_GRAYED);
                }
                DrawMenuBar(hwnd);
                ((ColumnOp *)this)->fit_piles();
                suit_lock = 0;
                InvalidateRect(hwnd, 0, TRUE);
                ((LayoutWin *)this)->center_rect(&r);
                InvalidateRect(hwnd, &r, FALSE);
                return;
            }
            CloseHandle(h);
            if (buf != 0)
                GlobalFree(buf);
        }
    }
    alert_box(hwnd, 0x12, 2, 0);
}
