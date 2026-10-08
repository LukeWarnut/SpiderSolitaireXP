#include "game_api.h"

void GameWin::show_dialog_107()
{
    DialogBoxParamW(
        g_hinst,
        MAKEINTRESOURCE(0x6b),
        hwnd,
        fn_010070D0,
        (LPARAM)this);
}
