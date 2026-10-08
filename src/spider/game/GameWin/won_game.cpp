#include "game_api.h"
#include <mmsystem.h>

void GameWin::won_game()
{
    HMENU menu;
    BOOL (WINAPI *pEnable)(HMENU, UINT, UINT);
    int ok;

    suit_lock = 1;
    menu = GetMenu(hwnd);
    pEnable = EnableMenuItem;
    pEnable(menu, 0x9c4d, MF_GRAYED);
    pEnable(menu, 0x9c4b, MF_GRAYED);
    tally(1);
    anim->reset_draw();
    paint_hdc(anim->hdcA);
    ((BlitBoard *)anim)->blit();
    if (play_snd != 0) {
        PlaySoundW(
            MAKEINTRESOURCE(0x81),
            GetModuleHandleW(0),
            0x40007);
    }
    {
        HWND parent = hwnd;
        ok = DialogBoxParamW(
            g_hinst,
            MAKEINTRESOURCE(0x82),
            parent,
            fn_01002E68,
            0);
    }
    if (ok == 1) {
        PostMessageW(hwnd, WM_COMMAND, 0x9c45, 0);
    }
}
