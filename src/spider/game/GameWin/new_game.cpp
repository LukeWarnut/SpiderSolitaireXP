#include "game_api.h"
#include <mmsystem.h>

#pragma intrinsic(memset)

void GameWin::new_game(int seed)
{
    int pass;
    int pile;
    int n;
    HMENU menu;

    if (suit_lock == 0)
        tally(0);
    suit_lock = 0;
    this->seed = seed;
    ((TripleTable *)mode)->shuffle(seed);
    board->clear_lists();
    f58 = 0;
    f5c = 0;
    f60 = 0;
    f64 = 0;
    disable_undo_menu();
    in_play = 0;
    suit_src = 0;
    add_score(500);
    memset(z, 0, sizeof(z));
    InvalidateRect(hwnd, 0, TRUE);
    UpdateWindow(hwnd);
    n = 0;
    for (pass = 0; pass < 5; pass++) {
        for (pile = 0; pile < 10; pile++) {
            if (pass != 4 || pile <= 3) {
                ((TripleTable *)mode)->next();
                ((TripleTable *)mode)->set_suit(n, 0);
                board->add_card(pile, n, 0);
                n++;
            }
        }
    }
    InvalidateRect(hwnd, 0, TRUE);
    if (use_fx != 0)
        UpdateWindow(hwnd);
    for (pile = 0; pile < 10; pile++) {
        ((TripleTable *)mode)->next();
        ((TripleTable *)mode)->set_suit(n, 1);
        board->add_card(pile, n, 1);
        if (use_fx != 0) {
            anim_deal(pile, (pile < 4) + 4);
            if (play_snd != 0)
                PlaySoundW((LPCWSTR)0x7c, GetModuleHandleW(0), 0x40007);
        }
        n++;
    }
    level = n;
    menu = GetMenu(hwnd);
    EnableMenuItem(menu, 0x9c46, 0);
    EnableMenuItem(menu, 0x9c47, 0);
    EnableMenuItem(menu, 0x9c50, 0);
    EnableMenuItem(menu, 0x9c4d, 0);
    EnableMenuItem(menu, 0x9c4b, 0);
    DrawMenuBar(hwnd);
}
