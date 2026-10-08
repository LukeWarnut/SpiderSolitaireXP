#include "game_api.h"

void GameWin::deal_prompt()
{
    int ans;

    /* A fresh hwnd local at each call keeps the reload in eax. */
    if (in_play > 0 && level > 0 && suit_lock == 0) {
        HWND parent = hwnd;
        ans = alert_box(parent, 0x32, 2, 0x223);
        if (ans == 2) {
            return;
        }
        if (ans == 6) {
            save_game();
        }
    }
    HWND parent = hwnd;
    if (DialogBoxParamW(
            g_hinst,
            MAKEINTRESOURCE(0x77),
            parent,
            fn_010075D4,
            (LPARAM)this) == 2) {
        return;
    }
    seed_now();
}
