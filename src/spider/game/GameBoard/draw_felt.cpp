#include "game_api.h"

void GameBoard::draw_felt(HDC hdc, int x0, int y0, RECT *clip, RECT *rc)
{
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = LoadBitmapW(g_hinst, L"FELT");
    HGDIOBJ old = SelectObject(mem, bmp);
    int x_first = 0x3f - (rc->left - clip->left) % 0x3f;
    int x_last = (rc->right - rc->left - x_first) % 0x3f;
    int y_first = 0x40 - (rc->top - clip->top) % 0x40;
    int y_last = (rc->bottom - rc->top - y_first) % 0x40;
    int x, y, cx, cy, sx, sy;

    for (y = y0; y < rc->bottom - rc->top && y < clip->bottom; y += cy) {
        if (y < y0 + y_first) {
            cy = y_first;
            sy = 0x40 - y_first;
        } else {
            sy = 0;
            cy = y_last;
            if (y != rc->bottom - rc->top - y_last - y0)
                cy = 0x40;
        }
        for (x = x0; x < rc->right - rc->left && x < clip->right; x += cx) {
            if (x < x_first + x0) {
                sx = 0x3f - x_first;
                cx = x_first;
            } else {
                cx = x_last;
                sx = 0;
                if (x != rc->right - rc->left - x_last - x0)
                    cx = 0x3f;
            }
            BitBlt(hdc, x, y, cx, cy, mem, sx, sy, SRCCOPY);
        }
    }
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}
