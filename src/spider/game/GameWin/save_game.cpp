#include "game_api.h"

int GameWin::save_game()
{
    DWORD put;
    HANDLE h;
    int pile;
    int k;
    int *buf;
    int i;

    i = 6;
    if (opt_f6c != 0 && saved != 0) {
        HWND w = hwnd;
        if (alert_box(w, 0x10, 2, 0x124) != IDYES)
            goto out;
    }
    h = open_saved(GENERIC_WRITE, CREATE_ALWAYS);
    if (h == INVALID_HANDLE_VALUE)
        goto fail;
    buf = (int *)GlobalAlloc(0, 0x800);
    if (buf == 0) {
        CloseHandle(h);
fail:
        alert_box(hwnd, 0x12, 2, 0);
out:
        return 0;
    }
    buf[0] = ((TripleTable *)mode)->mode;
    buf[1] = seed;
    buf[2] = ((TripleTable *)mode)->idx;
    buf[3] = level;
    buf[4] = in_play;
    buf[5] = f58;
    for (k = 0; k < 4; k++)
        buf[i++] = z[k];
    for (k = 0; k < 8; k++)
        buf[i++] = z2[k];
    for (pile = 0; pile < 10; pile++) {
        buf[i++] = board->counts[pile];
        buf[i++] = board->extras[pile];
        for (k = 0; k < board->counts[pile]; k++)
            buf[i++] = ((PileTable *)board)->card_value(pile, k);
    }
    buf[i++] = suit_src;
    WriteFile(h, buf, i * 4, &put, 0);
    GlobalFree(buf);
    CloseHandle(h);
    check_saved();
    return 1;
}
