#include "game_api.h"
#include <new>

LRESULT GameWin::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    HDC hdc;
    int span;
    int ans;
    int fx;
    int fy;
    int menu;
    int cap;
    AnimState *p;

    switch (msg) {
    case WM_CREATE:
        p = (AnimState *)::operator new(0x398c);
        anim = p != 0 ? p->init_draw(hwnd) : 0;
        return 0;
    case WM_MOVE:
        if (IsIconic(this->hwnd))
            iconic = 1;
        break;
    case WM_SIZE:
        span = (LOWORD(lp) - 0x2c6) / 0xb;
        a = span;
        if (span < 0)
            span = 0;
        b = span;
        ((ColumnOp *)this)->fit_piles();
        if (anim->b != 0) {
            UpdateWindow(hwnd);
            anim->shutdown();
            paint_hdc(*(HDC *)((char *)anim + 0x3974));
            ((BlitBoard *)anim)->blit();
        }
        return 0;
    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        if (anim->b != 0)
            ((SurfaceBlit *)anim)->blit_to(hdc);
        else
            paint_hdc(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    case WM_CLOSE:
        save_settings();
        if (suit_lock == 0) {
            if (opt_f64 != 0) {
                if (!save_game())
                    tally(0);
            } else {
                HWND w = this->hwnd;
                ans = alert_box(w, 0x32, 2, 0x223);
                if (ans == IDYES) {
                    if (!save_game())
                        return 0;
                } else if (ans == IDNO) {
                    tally(0);
                } else {
                    return 0;
                }
            }
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_ACTIVATEAPP:
        active = (int)wp;
        if (iconic != 0 && wp != 0)
            iconic = 0;
        break;
    case WM_GETMINMAXINFO:
        fx = GetSystemMetrics(SM_CXFRAME);
        fy = GetSystemMetrics(SM_CYFRAME);
        menu = GetSystemMetrics(SM_CYMENU);
        cap = GetSystemMetrics(SM_CYCAPTION);
        ((MINMAXINFO *)lp)->ptMinTrackSize.x = fx + fx + 0x258;
        ((MINMAXINFO *)lp)->ptMinTrackSize.y = cap + fy * 2 + 0x190 + menu;
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            ShowWindow(this->hwnd, SW_MINIMIZE);
            return 0;
        }
        break;
    case WM_COMMAND:
        return on_command(hwnd, msg, wp, lp);
    case WM_MOUSEMOVE:
        return ((GameBoard *)this)->on_mouse_move(hwnd, msg, wp, lp);
    case WM_LBUTTONDOWN:
        return ((GameBoard *)this)->on_left_down(hwnd, msg, wp, lp);
    case WM_LBUTTONUP:
        return ((GameBoard *)this)->on_left_up(hwnd, msg, wp, lp);
    case WM_RBUTTONDOWN:
        return ((GameBoard *)this)->on_right_down(hwnd, msg, wp, lp);
    case WM_RBUTTONUP:
        return ((GameBoard *)this)->on_right_up(hwnd, msg, wp, lp);
    case WM_DESTROY:
        if (anim != 0) {
            anim->release(1);
            anim = 0;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
