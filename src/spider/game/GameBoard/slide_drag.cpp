#include "game_api.h"
#include <mmsystem.h>

void GameBoard::slide_drag(HDC hdc0, HDC hdc1, HDC hdc2, HDC hdc3, RECT *src, RECT *dst, int)
{
    DWORD t0;
    DWORD now;
    unsigned elapsed;
    float scale;
    float fdx;
    float fdy;
    int dx;
    int dy;
    RECT *pn;
    RECT cur;
    RECT next;

    t0 = timeGetTime();
    dx = dst->left - src->left;
    dy = dst->top - src->top;
    fdx = (float)dx;
    fdy = (float)dy;
    cur = *src;
    goto loop_check;
    for (;;) {
        elapsed = now - t0;
        scale = (float)elapsed * 0.01f;
        pn = &next;
        pn->left = src->left + (int)(fdx * scale);
        pn->right = (pn->left - src->left) + src->right;
        pn->top = src->top + (int)(fdy * scale);
        pn->bottom = (pn->top - src->top) + src->bottom;
        refresh_drag((int)hdc0, (int)hdc3, (int)hdc2, (int)hdc1, &cur, &next);
        cur = next;
        Sleep(5);
    loop_check:
        now = timeGetTime();
        if (now > t0 + 100u)
            break;
    }
    next = *dst;
    refresh_drag((int)hdc0, (int)hdc3, (int)hdc2, (int)hdc1, &cur, &next);
}
