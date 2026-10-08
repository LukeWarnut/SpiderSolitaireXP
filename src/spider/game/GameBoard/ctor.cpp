#include "game_api.h"

GameBoard::GameBoard()
{
    int *p;
    int n;
    int v;

    n = 10;
    attrs = 0;
    board = 0;
    suit_lock = 1;
    f78 = 0;
    f7c = 0;
    f20 = 0;
    f24 = 0;
    p = extras;
    v = 0x1c;
    while (n != 0) {
        *p = v;
        p++;
        n--;
    }
    f58 = 0;
    in_play = 0;
    for (n = 0; n < 4; n++) {
        z[n] = 0;
    }
    reset_attrs();
    reset_board();
    hwnd = 0;
    HtmlHelpW(0, 0, HH_INITIALIZE, (DWORD_PTR)&hwnd);
    ((GameWin *)this)->load_settings();
}
