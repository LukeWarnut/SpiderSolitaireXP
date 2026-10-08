#include "game_api.h"

void GameWin::show_dialog_118()
{
    DialogBoxParamW(
        g_hinst,
        MAKEINTRESOURCE(0x76),
        hwnd,
        fn_010076A9,
        (LPARAM)this);
}
