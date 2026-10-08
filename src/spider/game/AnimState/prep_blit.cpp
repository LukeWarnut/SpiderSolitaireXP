#include "game_api.h"
#include <math.h>
#include <mmsystem.h>

void AnimState::prep_blit()
{
    unsigned rem;
    float x;
    float frac;
    float red;
    float green;
    float blue;
    int i;
    SIZE sz;
    RECT rc;
    int w;
    int h;

    rem = (timeGetTime() - t0) % 10000u;
    x = (float)rem;
    x = x * 0.0001f;
    x = x * 6.0f;
    i = (int)floor((double)x);
    frac = x - (float)i;
    switch (i) {
    case 0:
        red = 1.0f;
        green = frac;
        blue = 0.0f;
        break;
    case 1:
        red = 1.0f - frac;
        green = 1.0f;
        blue = 0.0f;
        break;
    case 2:
        red = 0.0f;
        green = 1.0f;
        blue = frac;
        break;
    case 3:
        red = 0.0f;
        green = 1.0f - frac;
        blue = 1.0f;
        break;
    case 4:
        red = frac;
        green = 0.0f;
        blue = 1.0f;
        break;
    case 5:
        red = 1.0f;
        green = 0.0f;
        blue = 1.0f - frac;
        break;
    }
    GetTextExtentPoint32W(hdcB, win_text, lstrlenW(win_text), &sz);
    h = dirty.bottom - dirty.top;
    w = dirty.right - dirty.left;
    SetRect(
        &rc,
        (w - sz.cx) / 2,
        (h - sz.cy) / 2,
        (sz.cx + w) / 2,
        (sz.cy + h) / 2);
    SetTextColor(
        hdcB,
        RGB((int)(red * 255.0f), (int)(green * 255.0f), (int)(blue * 255.0f)));
    SetBkMode(hdcB, TRANSPARENT);
    DrawTextW(hdcB, win_text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    UnionRect(&rc28, &rc28, &rc);
}
