#include "game_api.h"

void GameBoard::draw_card(void *hdc, int x, int y, int h, int a, int b)
{
    wchar_t name[0x14];
    HBITMAP bmp;
    HDC mem;
    HGDIOBJ old;

    if (h >= 1 && h <= 0x34) {
        wsprintfW(name, L"CARD%d", h);
        bmp = LoadBitmapW(g_hinst, name);
    } else if (h == 0x69) {
        bmp = LoadBitmapW(g_hinst, L"FELT");
    } else if (h == 0x68) {
        bmp = LoadBitmapW(g_hinst, L"CARDBACK");
    } else {
        bmp = LoadBitmapW(g_hinst, MAKEINTRESOURCE(h));
    }
    mem = CreateCompatibleDC((HDC)hdc);
    old = SelectObject(mem, bmp);
    BitBlt((HDC)hdc, x, y, 0x47, 0x60, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    if (a != 0) {
        SetPixel((HDC)hdc, x, y, 0x8000);
        SetPixel((HDC)hdc, x + 1, y, 0x8000);
        SetPixel((HDC)hdc, x, y + 1, 0x8000);
        SetPixel((HDC)hdc, x + 0x46, y, 0x8000);
        SetPixel((HDC)hdc, x + 0x45, y, 0x8000);
        SetPixel((HDC)hdc, x + 0x46, y + 1, 0x8000);
    }
    if (b != 0) {
        SetPixel((HDC)hdc, x, y + 0x5f, 0x8000);
        SetPixel((HDC)hdc, x + 1, y + 0x5f, 0x8000);
        SetPixel((HDC)hdc, x, y + 0x5e, 0x8000);
        SetPixel((HDC)hdc, x + 0x46, y + 0x5f, 0x8000);
        SetPixel((HDC)hdc, x + 0x45, y + 0x5f, 0x8000);
        SetPixel((HDC)hdc, x + 0x46, y + 0x5e, 0x8000);
    }
    if ((h >= 0xe && h <= 0x17) || (h >= 0x1b && h <= 0x24)) {
        MoveToEx((HDC)hdc, x, y + 2, 0);
        LineTo((HDC)hdc, x, y + 0x5d);
        LineTo((HDC)hdc, x + 2, y + 0x5f);
        LineTo((HDC)hdc, x + 0x44, y + 0x5f);
        LineTo((HDC)hdc, x + 0x46, y + 0x5d);
        LineTo((HDC)hdc, x + 0x46, y + 2);
        LineTo((HDC)hdc, x + 0x44, y);
        LineTo((HDC)hdc, x + 2, y);
        LineTo((HDC)hdc, x, y + 2);
    }
}
