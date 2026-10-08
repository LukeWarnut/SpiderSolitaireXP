#include "game_api.h"

extern "C" INT_PTR CALLBACK fn_01002E68(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    MSG peek;
    RECT parent_rc;
    RECT wnd_rc;
    int w;
    int h;

    (void)lp;

    switch (msg) {
    case WM_INITDIALOG:
        GetWindowRect(GetParent(hwnd), &parent_rc);
        GetWindowRect(hwnd, &wnd_rc);
        w = wnd_rc.right - wnd_rc.left;
        h = wnd_rc.bottom - wnd_rc.top;
        MoveWindow(
            hwnd,
            parent_rc.right - w - 0x14,
            parent_rc.bottom - h - 0xa,
            w,
            h,
            TRUE);
        SetTimer(hwnd, 1, 0x14, 0);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK:
            EndDialog(hwnd, IDOK);
            break;
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        default:
            return FALSE;
        }
        return TRUE;
    case WM_TIMER:
        g_game.anim->paint();
        while (PeekMessageW(&peek, hwnd, WM_TIMER, WM_TIMER, PM_REMOVE)) {
        }
        break;
    }
    return FALSE;
}
