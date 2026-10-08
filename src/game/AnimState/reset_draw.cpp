#include "game_api.h"
#include <mmsystem.h>
#include <string.h>

#pragma intrinsic(memset)

void AnimState::reset_draw()
{
    HDC hdc;
    HDC (WINAPI *pCreateCompatibleDC)(HDC);
    HGDIOBJ (WINAPI *pSelectObject)(HDC, HGDIOBJ);
    int (WINAPI *pLoadStringW)(HINSTANCE, UINT, LPWSTR, int);
    LOGFONTW lf;

    b = 1;
    GetClientRect(hwnd, &dirty);
    hdc = GetDC(hwnd);
    pCreateCompatibleDC = CreateCompatibleDC;
    hdcA = pCreateCompatibleDC(hdc);
    bmpA = CreateCompatibleBitmap(hdc, dirty.right, dirty.bottom);
    pSelectObject = SelectObject;
    oldA = pSelectObject(hdcA, bmpA);
    hdcB = pCreateCompatibleDC(hdc);
    bmpB = CreateCompatibleBitmap(hdc, dirty.right, dirty.bottom);
    oldB = pSelectObject(hdcB, bmpB);
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -MulDiv(
        0x30,
        GetDeviceCaps(hdcB, LOGPIXELSY),
        72);
    lf.lfWeight = 0x320;
    lf.lfCharSet = 1;
    pLoadStringW = LoadStringW;
    pLoadStringW(0, 0x30, lf.lfFaceName, 0x20);
    gdi100 = CreateFontIndirectW(&lf);
    gdi104 = pSelectObject(hdcB, gdi100);
    burst_fx(fx);
    burst_fx(fx + 1);
    ReleaseDC(hwnd, hdc);
    t0 = timeGetTime();
    SetRect(&rc18, 0, 0, 0, 0);
    pLoadStringW(0, 0x2d, win_text, 0x64);
}
