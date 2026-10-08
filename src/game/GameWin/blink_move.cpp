#include "game_api.h"
#include <mmsystem.h>

/* Same body as UndoRing/next.cpp, which is "link": false. Only a non-inline
 * definition in this TU lets /O1 keep ecx across the call. */
#pragma auto_inline(off)
UndoRec *UndoRing::next()
{
    int i = idx;
    int n = i + 1;
    idx = n;
    if (n == limit) {
        idx = 0;
    }
    return &items[i];
}

void GameWin::blink_move()
{
    RECT rc;
    HDC hdc;
    Rec24 *rec;

    if (*(int *)((char *)this + 0x5C) == 0) {
        collect_moves();
        ((RecBank *)this)->sort_desc();
    }
    if (*(int *)((char *)this + 0x60) == 0) {
        if (play_snd != 0) {
            PlaySoundW(MAKEINTRESOURCE(0x7F), GetModuleHandleW(0), 0x40007);
        }
        return;
    }
    hdc = GetDC(hwnd);
    if (play_snd != 0) {
        PlaySoundW(MAKEINTRESOURCE(0x7E), GetModuleHandleW(0), 0x40007);
    }
    rec = (Rec24 *)((UndoRing *)this)->next();
    ((ColumnOp *)this)->place_on_top(rec->b, rec->c, (int)&rc);
    InvertRect(hdc, &rc);
    GdiFlush();
    Sleep(0xFA);
    InvertRect(hdc, &rc);
    ((ColumnOp *)this)->place_on_top(rec->d, rec->e, (int)&rc);
    InvertRect(hdc, &rc);
    GdiFlush();
    Sleep(0xFA);
    InvertRect(hdc, &rc);
    ReleaseDC(hwnd, hdc);
}
#pragma auto_inline(on)

