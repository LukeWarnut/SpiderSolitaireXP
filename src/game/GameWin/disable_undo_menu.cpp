#include "game_api.h"

void GameWin::disable_undo_menu()
{
    HMENU menu;

    undo_n = 0;
    menu = GetMenu(hwnd);
    EnableMenuItem(menu, 0x9c4a, MF_GRAYED);
}
