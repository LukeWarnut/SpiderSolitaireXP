#include "game_api.h"

extern "C" void __stdcall fn_010071F3(HWND hwnd, GameWin *g)
{
    wchar_t buf[0x400];
    HWND tab;
    int sel;
    int won;
    double pct;

    if (g == 0)
        return;
    tab = GetDlgItem(hwnd, 0x3f3);
    sel = (int)SendMessageW(tab, 0x130b, 0, 0);
    wsprintfW(buf, load_string(0x21), g->suit_slot[sel][0]);
    SetDlgItemTextW(hwnd, 0x3ee, buf);
    wsprintfW(buf, load_string(0x22), g->suit_slot[sel][1]);
    SetDlgItemTextW(hwnd, 0x3eb, buf);
    wsprintfW(buf, load_string(0x23), g->suit_slot[sel][2]);
    SetDlgItemTextW(hwnd, 0x3ec, buf);
    pct = 0;
    won = g->suit_slot[sel][1];
    if (won > 0)
        pct = (double)won / (double)(g->suit_slot[sel][2] + won) * 100.0;
    wsprintfW(buf, load_string(0x29), (int)pct);
    SetDlgItemTextW(hwnd, 0x3ed, buf);
    wsprintfW(buf, load_string(0x24), g->suit_slot[sel][3]);
    SetDlgItemTextW(hwnd, 0x3ef, buf);
    wsprintfW(buf, load_string(0x25), g->suit_slot[sel][4]);
    SetDlgItemTextW(hwnd, 0x3f0, buf);
    if (g->suit_slot[sel][6] != 0)
        wsprintfW(buf, load_string(0x26), g->suit_slot[sel][5]);
    else
        wsprintfW(buf, load_string(0x27), g->suit_slot[sel][5]);
    SetDlgItemTextW(hwnd, 0x3f1, buf);
}
