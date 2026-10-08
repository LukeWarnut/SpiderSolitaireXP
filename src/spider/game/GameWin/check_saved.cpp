#include "game_api.h"

void GameWin::check_saved()
{
    HANDLE file;
    HMENU menu;

    file = open_saved(0x80000000, 3);
    menu = GetMenu(hwnd);
    if (file != INVALID_HANDLE_VALUE) {
        EnableMenuItem(menu, 0x9c4c, 0);
        saved = 1;
        CloseHandle(file);
    } else {
        EnableMenuItem(menu, 0x9c4c, 1);
        saved = 0;
    }
}
