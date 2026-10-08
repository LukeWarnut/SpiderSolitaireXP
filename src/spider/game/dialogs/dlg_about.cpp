#include "game_api.h"

extern "C" INT_PTR CALLBACK fn_010070D0(HWND hwnd, UINT msg, WPARAM wp, LPARAM)
{
    wchar_t buf[0x400];
    RECT rc;
    HDC hdc;
    HDC mem;
    HBITMAP bmp;
    HGDIOBJ old;
    HGDIOBJ (WINAPI *pSelect)(HDC, HGDIOBJ);

    switch (msg) {
    case WM_PAINT:
        hdc = GetDC(hwnd);
        rc.left = 0x19;
        rc.top = 0x6e;
        rc.right = 0x109;
        rc.bottom = 0xc8;
        mem = CreateCompatibleDC(hdc);
        bmp = LoadBitmapW(g_hinst, MAKEINTRESOURCE(0x6a));
        pSelect = SelectObject;
        old = pSelect(mem, bmp);
        BitBlt(hdc, 0xa, 0xa, 0x167, 0xf2, mem, 0, 0, SRCCOPY);
        pSelect(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        lstrcpyW(buf, load_string(0xf));
        old = pSelect(hdc, GetStockObject(0x11));
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, buf, -1, &rc, DT_WORDBREAK);
        pSelect(hdc, old);
        ReleaseDC(hwnd, hdc);
        return FALSE;
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
    case WM_INITDIALOG:
        return TRUE;
    default:
        return FALSE;
    }
}
