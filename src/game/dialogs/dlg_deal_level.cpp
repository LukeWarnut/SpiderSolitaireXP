#include "game_api.h"

extern "C" GameWin *g_level_game;
GameWin *g_level_game;

extern "C" INT_PTR CALLBACK fn_010075D4(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    UINT (WINAPI *pChecked)(HWND, int);
    int level;
    UINT idFirst;

    switch (msg) {
    case WM_INITDIALOG:
        g_level_game = (GameWin *)lp;
        level = *g_level_game->mode;
        if (level == 1) {
            idFirst = 0x3f0;
            CheckRadioButton(hwnd, idFirst, 0x3f2, idFirst);
        } else if (level == 2) {
            CheckRadioButton(hwnd, 0x3f0, 0x3f2, 0x3f1);
        } else {
            CheckRadioButton(hwnd, 0x3f0, 0x3f2, 0x3f2);
        }
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case 1:
            pChecked = IsDlgButtonChecked;
            if (pChecked(hwnd, 0x3f0)) {
                *g_level_game->mode = 1;
            } else if (pChecked(hwnd, 0x3f1)) {
                *g_level_game->mode = 2;
            } else {
                *g_level_game->mode = 4;
            }
            EndDialog(hwnd, 1);
            return TRUE;
        case 2:
            EndDialog(hwnd, 2);
            return TRUE;
        default:
            return FALSE;
        }
    default:
        return FALSE;
    }
}
