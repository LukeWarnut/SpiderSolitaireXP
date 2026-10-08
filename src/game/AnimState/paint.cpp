#include "game_api.h"

void AnimState::paint()
{
    RECT uni;
    HDC hdc;
    FxBank *bank;
    int n;

    BitBlt(
        hdcB,
        rc28.left,
        rc28.top,
        rc28.right - rc28.left,
        rc28.bottom - rc28.top,
        hdcA,
        rc28.left,
        rc28.top,
        SRCCOPY);
    rc18 = rc28;
    SetRect(&rc28, 0, 0, 0, 0);
    prep_blit();
    bank = fx;
    n = 2;
    do {
        run_fx(bank);
        tick_fx(bank);
        bank++;
        n--;
    } while (n != 0);
    UnionRect(&uni, &rc18, &rc28);
    hdc = GetDC(hwnd);
    BitBlt(
        hdc,
        uni.left,
        uni.top,
        uni.right - uni.left,
        uni.bottom - uni.top,
        hdcB,
        uni.left,
        uni.top,
        SRCCOPY);
    ReleaseDC(hwnd, hdc);
}
