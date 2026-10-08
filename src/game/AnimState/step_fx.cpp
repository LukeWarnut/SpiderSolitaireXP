#include "game_api.h"

void AnimState::step_fx(FxItem *item)
{
    int h;
    int w;
    int s;
    int xoff;
    int yoff;
    int cx;
    int cy;
    int r;
    RECT rc;
    HBRUSH br;
    HGDIOBJ old;
    HGDIOBJ (WINAPI *pSelectObject)(HDC, HGDIOBJ);

    h = dirty.bottom - dirty.top;
    w = dirty.right - dirty.left;
    s = h;
    if (w <= h) {
        s = w;
    }
    s = s / 0x32;
    xoff = (int)((float)s * item->x);
    yoff = (int)((float)s * item->y);
    cx = dirty.left + xoff + w / 2;
    cy = dirty.bottom + h / -2 - yoff;
    r = (int)(item->scale * 12.0f) / 2;
    SetRect(&rc, cx - r, cy - r, cx + r, cy + r);
    br = CreateSolidBrush(
        RGB((int)(item->r * 255.0f), (int)(item->g * 255.0f), (int)(item->b * 255.0f)));
    pSelectObject = SelectObject;
    old = pSelectObject(hdcB, br);
    Ellipse(hdcB, rc.left, rc.top, rc.right, rc.bottom);
    pSelectObject(hdcB, old);
    DeleteObject(br);
    UnionRect(&rc28, &rc28, &rc);
}
