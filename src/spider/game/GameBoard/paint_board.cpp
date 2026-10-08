#include "game_api.h"

void GameBoard::paint_board(HDC hdc, int dx, int dy)
{
    wchar_t buf[100];
    RECT t;
    RECT rc;
    HBRUSH fill;
    HBRUSH frame;
    int bk;
    COLORREF fg;

    ((LayoutWin *)this)->center_rect(&rc);
    OffsetRect(&rc, dx, dy);
    fill = CreateSolidBrush(0x7f00);
    frame = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (fill != 0) {
        FillRect(hdc, &rc, fill);
        DeleteObject(fill);
    }
    if (frame != 0)
        FrameRect(hdc, &rc, frame);
    bk = GetBkMode(hdc);
    fg = GetTextColor(hdc);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, 0xffffff);
    SetRect(&t, rc.left + 10, rc.top + 0x1e, rc.left + 100, rc.top + 0x32);
    DrawTextW(hdc, load_string(0x35), -1, &t, DT_SINGLELINE | DT_RIGHT);
    SetRect(&t, rc.left + 10, rc.top + 0x32, rc.left + 100, rc.top + 0x50);
    DrawTextW(hdc, load_string(0x36), -1, &t, DT_SINGLELINE | DT_RIGHT);
    wsprintfW(buf, L"%d", ((GameWin *)this)->suit_src);
    SetRect(&t, rc.left + 0x6e, rc.top + 0x1e, rc.left + 200, rc.top + 0x32);
    DrawTextW(hdc, buf, -1, &t, DT_SINGLELINE);
    wsprintfW(buf, L"%d", ((GameWin *)this)->in_play);
    SetRect(&t, rc.left + 0x6e, rc.top + 0x32, rc.left + 200, rc.top + 0x50);
    DrawTextW(hdc, buf, -1, &t, DT_SINGLELINE);
    SetBkMode(hdc, bk);
    SetTextColor(hdc, fg);
}
