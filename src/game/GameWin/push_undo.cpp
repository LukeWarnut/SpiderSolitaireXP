#include "game_api.h"

int GameWin::push_undo(int *rec)
{
    GameWin *edi;
    int esi;
    int ebx;
    int edx;
    int eax;
    HMENU menu;

    edi = this;
    esi = edi->undo_n;
    if (esi == 0) {
        menu = GetMenu(edi->hwnd);
        EnableMenuItem(menu, 0x9c4a, 0);
    }
    if (esi == 0x96) {
        ((UndoBuf *)edi)->pop_row();
        esi--;
    }
    edx = rec[0];
    eax = esi;
    eax = eax + eax * 4;
    eax = (int)edi + eax * 4;
    *((int *)(eax + 0x354)) = edx;
    edx = rec[1];
    *((int *)(eax + 0x358)) = edx;
    ebx = rec[2];
    edx = esi;
    edx = edx + edx * 4 + 0xd7;
    *((int *)((char *)edi + edx * 4)) = ebx;
    edx = rec[3];
    *((int *)(eax + 0x360)) = edx;
    ebx = rec[4];
    esi++;
    *((int *)(eax + 0x364)) = ebx;
    edi->undo_n = esi;
    eax = esi;
    return eax;
}
