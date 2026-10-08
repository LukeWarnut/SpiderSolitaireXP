#include "game_api.h"

void GameWin::show_dialog_117()
{
    DialogBoxParamW(
        g_hinst,
        MAKEINTRESOURCE(0x75),
        hwnd,
        fn_010073B6,
        (LPARAM)this);
}
