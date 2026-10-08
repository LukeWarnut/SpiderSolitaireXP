#include "game_api.h"

void GameWin::paint_hdc(HDC hdc)
{
    RECT rc;
    wchar_t buf[20];
    COLORREF px[12];
    HDC card_dc;
    HDC back_dc;
    HBITMAP back;
    HGDIOBJ old_back;
    HBITMAP bmp;
    HGDIOBJ old;
    int pile;
    int n;
    int cap;
    int x;
    int y;
    int i;
    int last;
    int code;
    int ax;
    int bx;
    int h;

    GetClientRect(hwnd, &rc);
    ((GameBoard *)this)->draw_felt(hdc, 0, 0, &rc, &rc);
    card_dc = CreateCompatibleDC(hdc);
    back_dc = CreateCompatibleDC(hdc);
    back = LoadBitmapW(g_hinst, L"CARDBACK");
    old_back = SelectObject(back_dc, back);
    for (pile = 0; pile < 10; pile++) {
        n = board->counts[pile];
        cap = board->extras[pile];
        x = (a + 0x47) * pile + b;
        ax = a;
        bx = b;
        if (n == 0) {
            if (ax < 0 && pile > 0) {
                h = ((LayoutWin *)this)->deal_top();
                ExcludeClipRect(hdc, (ax + 0x47) * (pile - 1) + bx, 0,
                    (ax + 0x47) * (pile - 1) + 0x47 + bx, h);
            }
            ((GameBoard *)this)->draw_card(hdc, x, 10, 0x6c, 1, 1);
            continue;
        }
        for (i = 0; i < n; i++) {
            last = i >= n - 1;
            code = ((DealView *)this)->card_code(pile, i);
            if (i < cap) {
                y = i * 7 + 10;
            } else {
                y = ((GameBoard *)this)->extras[pile] * (i - cap) + cap * 7 + 10;
            }
            px[0] = GetPixel(hdc, x, y);
            px[1] = GetPixel(hdc, x + 1, y);
            px[2] = GetPixel(hdc, x, y + 1);
            px[3] = GetPixel(hdc, x + 0x46, y);
            px[4] = GetPixel(hdc, x + 0x45, y);
            px[5] = GetPixel(hdc, x + 0x46, y + 1);
            if (last) {
                px[6] = GetPixel(hdc, x, y + 0x5f);
                px[7] = GetPixel(hdc, x + 1, y + 0x5f);
                px[8] = GetPixel(hdc, x, y + 0x5e);
                px[9] = GetPixel(hdc, x + 0x46, y + 0x5f);
                px[10] = GetPixel(hdc, x + 0x45, y + 0x5f);
                px[11] = GetPixel(hdc, x + 0x46, y + 0x5e);
            }
            if (code == 0x68) {
                if (!last) {
                    BitBlt(hdc, x, y, 0x47, 9, back_dc, 0, 0, SRCCOPY);
                } else {
                    BitBlt(hdc, x, y, 0x47, 0x60, back_dc, 0, 0, SRCCOPY);
                }
            } else {
                if (code >= 1 && code <= 0x34) {
                    wsprintfW(buf, L"CARD%d", code);
                    bmp = LoadBitmapW(g_hinst, buf);
                } else if (code == 0x69) {
                    bmp = LoadBitmapW(g_hinst, L"FELT");
                } else {
                    bmp = LoadBitmapW(g_hinst, MAKEINTRESOURCEW(code));
                }
                old = SelectObject(card_dc, bmp);
                BitBlt(hdc, x, y, 0x47, 0x60, card_dc, 0, 0, SRCCOPY);
                SelectObject(card_dc, old);
                DeleteObject(bmp);
            }
            SetPixel(hdc, x, y, px[0]);
            SetPixel(hdc, x + 1, y, px[1]);
            SetPixel(hdc, x, y + 1, px[2]);
            SetPixel(hdc, x + 0x46, y, px[3]);
            SetPixel(hdc, x + 0x45, y, px[4]);
            SetPixel(hdc, x + 0x46, y + 1, px[5]);
            if (last) {
                SetPixel(hdc, x, y + 0x5f, px[6]);
                SetPixel(hdc, x + 1, y + 0x5f, px[7]);
                SetPixel(hdc, x, y + 0x5e, px[8]);
                SetPixel(hdc, x + 0x46, y + 0x5f, px[9]);
                SetPixel(hdc, x + 0x45, y + 0x5f, px[10]);
                SetPixel(hdc, x + 0x46, y + 0x5e, px[11]);
            }
            if ((code >= 0xe && code <= 0x17) || (code >= 0x1b && code <= 0x24)) {
                MoveToEx(hdc, x, y + 2, 0);
                LineTo(hdc, x, y + 0x5d);
                LineTo(hdc, x + 2, y + 0x5f);
                LineTo(hdc, x + 0x44, y + 0x5f);
                LineTo(hdc, x + 0x46, y + 0x5d);
                LineTo(hdc, x + 0x46, y + 2);
                LineTo(hdc, x + 0x44, y);
                LineTo(hdc, x + 2, y);
                LineTo(hdc, x, y + 2);
            }
        }
    }
    SelectObject(back_dc, old_back);
    DeleteObject(back);
    DeleteDC(back_dc);
    DeleteDC(card_dc);
    ((GameBoard *)this)->paint_board(hdc, 0, 0);
    ((LayoutBox *)this)->draw_stock(hdc, 0, 0);
    draw_cleared(hdc, 0, 0);
}
