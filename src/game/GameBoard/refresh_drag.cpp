#include "game_api.h"
#include <stdlib.h>

#pragma intrinsic(abs)

void GameBoard::refresh_drag(int a, int b, int c, int d, RECT *old_rc, RECT *new_rc)
{
    HDC screen = (HDC)a;
    HDC save = (HDC)b;
    HDC work = (HDC)c;
    HDC card = (HDC)d;
    int w;
    int h;
    int dy;
    int dx;
    int ady;
    int adx;
    int ch;
    int wr;
    int y_dn;
    int y_up;
    int x_rt;
    int x_lt;

    w = old_rc->right - old_rc->left;
    h = old_rc->bottom - old_rc->top;
    dx = new_rc->left - old_rc->left;
    dy = new_rc->top - old_rc->top;
    ady = abs(dy);
    ch = h - ady;
    adx = abs(dx);
    y_dn = (dy > 0) * ady;
    y_up = (dy < 0) * ady;
    x_rt = (dx > 0) * adx;
    x_lt = (dx < 0) * adx;

    if (w > 0 && ady > 0) {
        if (dy > 0)
            BitBlt(save, 0, 0, w, ady, work, 0, 0, SRCCOPY);
        else if (dy < 0)
            BitBlt(save, 0, ch, w, ady, work, 0, ch, SRCCOPY);
    }
    if (adx > 0 && ch > 0) {
        if (dx > 0)
            BitBlt(save, 0, y_dn, adx, ch, work, 0, y_dn, SRCCOPY);
        else if (dx < 0)
            BitBlt(save, w - adx, y_dn, adx, ch, work, w - adx, y_dn, SRCCOPY);
    }
    wr = w - adx;
    if (wr + 1 > 0 && ch + 1 > 0)
        BitBlt(work, x_lt, y_up, wr + 1, ch + 1, work, x_rt, y_dn, SRCCOPY);
    if (w > 0 && ady > 0) {
        if (dy > 0) {
            BitBlt(work, 0, ch, w, ady, screen, new_rc->left, old_rc->bottom, SRCCOPY);
        } else if (dy < 0) {
            if (new_rc->top >= 0)
                BitBlt(work, 0, 0, w, ady, screen, new_rc->left, new_rc->top, SRCCOPY);
            else
                BitBlt(work, 0, -new_rc->top, w, ady + new_rc->top, screen, new_rc->left, 0, SRCCOPY);
        }
    }
    if (adx > 0 && ch > 0) {
        if (dx > 0) {
            BitBlt(work, wr, y_up, adx, ch, screen, old_rc->right, new_rc->top + y_up, SRCCOPY);
        } else if (dx < 0) {
            if (new_rc->left >= 0)
                BitBlt(work, 0, y_up, adx, ch, screen, new_rc->left, new_rc->top + y_up, SRCCOPY);
            else
                BitBlt(work, -new_rc->left, y_up, adx + new_rc->left, ch, screen, 0, new_rc->top + y_up, SRCCOPY);
        }
    }
    BitBlt(screen, new_rc->left, new_rc->top, w, h, card, 0, 0, SRCCOPY);
    if (w > 0 && ady > 0) {
        if (dy > 0)
            BitBlt(screen, old_rc->left, old_rc->top, w, ady, save, 0, 0, SRCCOPY);
        else if (dy < 0)
            BitBlt(screen, old_rc->left, new_rc->bottom, w, ady, save, 0, ch, SRCCOPY);
    }
    if (adx > 0 && ch > 0) {
        if (dx > 0) {
            BitBlt(screen, old_rc->left, old_rc->top + y_dn, adx, ch, save, 0, y_dn, SRCCOPY);
        } else if (dx < 0) {
            if (adx <= drag_x1)
                BitBlt(screen, old_rc->right - adx, old_rc->top + y_dn, adx, ch, save, drag_x1 - adx, y_dn, SRCCOPY);
            else
                BitBlt(screen, old_rc->left, old_rc->top + y_dn, drag_x1, ch, save, 0, y_dn, SRCCOPY);
        }
    }
}
