#include "game_api.h"

void AnimState::clear_draw()
{
    HGDIOBJ (WINAPI *pSelectObject)(HDC, HGDIOBJ);
    BOOL (WINAPI *pDeleteObject)(HGDIOBJ);
    BOOL (WINAPI *pDeleteDC)(HDC);
    HDC *p;

    pSelectObject = SelectObject;
    p = &hdcA;
    pSelectObject(*p, oldA);
    pDeleteObject = DeleteObject;
    pDeleteObject(bmpA);
    pDeleteDC = DeleteDC;
    pDeleteDC(*p);
    pSelectObject(hdcB, oldB);
    pSelectObject(hdcB, gdi104);
    pDeleteObject(gdi100);
    pDeleteObject(bmpB);
    pDeleteDC(hdcB);
    b = 0;
    InvalidateRect(hwnd, &dirty, 0);
}
