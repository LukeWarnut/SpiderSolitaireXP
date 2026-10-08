#include "game_api.h"
#include <mmsystem.h>

void GameWin::deal()
{
    RECT rc;
    int pile;
    int saved;
    int off;
    int idx;
    int home;
    int spill_off;
    int y;
    int aa;
    int bb;
    HDC hdc;

    if (f58 >= 5) {
        return;
    }
    if (((CardColumn *)((DealView *)this)->col)->any_empty() != 0) {
        alert_box(hwnd, 5, 2, 0);
        return;
    }
    pile = 0;
    saved = level;
    if (use_fx == pile) {
        if (play_snd != pile) {
            PlaySoundW((LPCWSTR)0x7c, GetModuleHandleW(0), 0x40007);
        }
        ((LayoutBox *)this)->fill_rect(&rc);
        InvalidateRect(hwnd, &rc, pile);
    }
    off = 0x28;
    saved += 10;
    spill_off = off;
    home = saved;
    do {
        idx = ((TripleTable *)mode)->idx;
        ((TripleTable *)mode)->next();
        ((TripleTable *)mode)->set_suit(idx, 1);
        ((TenListBoard *)((DealView *)this)->col)->add_card(pile, idx, 1);
        if (use_fx != 0) {
            idx = *(int *)(off + (char *)((DealView *)this)->col) - 1;
            anim_deal(pile, idx);
            if (play_snd != 0) {
                PlaySoundW((LPCWSTR)0x7c, GetModuleHandleW(0), 0x40007);
            }
            if (a < 0) {
                ((ColumnOp *)this)->place(pile, idx, idx, (int)&rc);
                InvalidateRect(hwnd, &rc, 0);
                UpdateWindow(hwnd);
            }
        } else {
            int last = *(int *)(off + (char *)((DealView *)this)->col) - 1;
            y = ((Layout *)this)->score(pile, last);
            aa = a;
            bb = b;
            hdc = GetDC(hwnd);
            ((GameBoard *)this)->draw_card(
                hdc,
                (aa + 0x47) * pile + bb,
                y,
                ((DealView *)this)->card_code(pile, last),
                1,
                1);
            ReleaseDC(hwnd, hdc);
            off = spill_off;
        }
        off += 4;
        spill_off = off;
        pile++;
    } while (off < 0x50);
    f58++;
    level = home;
    *(int *)((char *)this + 0x5c) = 0;
    *(int *)((char *)this + 0x60) = 0;
    *(int *)((char *)this + 0x64) = 0;
    disable_undo_menu();
    ((ColumnOp *)this)->fit_piles();
    if (f58 == 5) {
        HMENU menu = GetMenu(hwnd);
        EnableMenuItem(GetSubMenu(menu, 0), 0x9c47, 1);
        EnableMenuItem(menu, 0x9c50, 1);
        DrawMenuBar(hwnd);
    }
    off = 0;
    do {
        if (((DealView *)this)->full_suit(off) != 0) {
            take_suit(off);
            InvalidateRect(hwnd, 0, 0);
            UpdateWindow(hwnd);
        }
        off++;
    } while (off < 10);
}

/* any_empty's body has to be visible in this TU. Gold leaves ecx = col across
 * that call into alert. inline + auto_inline(off) still reloads ecx; a normal
 * definition here does not. The standalone unit is not linked (see units.json
 * "link": false) so this copy is the one in the image. */
#include "../CardColumn/any_empty.cpp"
