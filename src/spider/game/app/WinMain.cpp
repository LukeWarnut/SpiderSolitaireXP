#include "game_api.h"
#include <commctrl.h>
#include <string.h>
#pragma intrinsic(memset)

HINSTANCE g_hinst;

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    OSVERSIONINFOA ver;
    INITCOMMONCONTROLSEX icc;
    WNDCLASSW wc;
    HWND hwnd;
    HACCEL accel;
    MSG msg;
    int x;
    int y;
    int nWidth;
    int nHeight;
    const int cw = (int)0x80000000;

    (void)prev;
    (void)cmd;
    (void)show;

    g_hinst = inst;
    ver.dwOSVersionInfoSize = 0x94;
    GetVersionExA(&ver);
    if (ver.dwPlatformId == 1) {
        char text[300];
        char cap[300];

        LoadStringA(0, 0x37, text, 300);
        LoadStringA(0, 2, cap, 300);
        MessageBoxA(0, text, cap, 0);
        return 1;
    }

    memset(&icc, 0, sizeof(icc));
    icc.dwSize = 8;
    icc.dwICC = 8;
    InitCommonControlsEx(&icc);
    wc.style = 0x2003;
    wc.lpfnWndProc = wnd_proc_thunk;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(0x67));
    wc.hCursor = LoadCursorW(0, IDC_ARROW);
    wc.hbrBackground = 0;
    wc.lpszMenuName = MAKEINTRESOURCEW(0x65);
    wc.lpszClassName = load_string(1);
    RegisterClassW(&wc);
    x = g_game.wrect.left;
    y = g_game.wrect.top;
    nWidth = g_game.wrect.right == cw ? cw : g_game.wrect.right - x;
    nHeight = g_game.wrect.right == cw ? cw : g_game.wrect.bottom - y;
    hwnd = CreateWindowExW(0, load_string(1), load_string(2), 0x02cf0000,
                           x, y, nWidth, nHeight, 0, 0, inst, 0);
    g_game.hwnd = hwnd;
    g_game.check_saved();
    g_game.level |= -1;
    ShowWindow(hwnd, g_game.show);
    
    if (g_game.use_fx == 0) {
        UpdateWindow(hwnd);
    }
    if (g_game.opt_f68 == 0 || g_game.saved == 0) {
        PostMessageW(hwnd, WM_COMMAND, 0x9c51, 0);
    } else {
        accel = (HACCEL)g_game.opt_f70;
        g_game.opt_f70 = 0;
        g_game.load_game();
        g_game.opt_f70 = (int)accel;
    }
    accel = LoadAcceleratorsW(inst, MAKEINTRESOURCEW(0x66));

    for (;;) {
        AnimState *st;

        st = g_game.anim;
        if (st == 0 || st->b == 0) {
            if (GetMessageW(&msg, 0, 0, 0) == 0) {
                break;
            }
            if (HtmlHelpW(0, 0, HH_PRETRANSLATEMESSAGE, (DWORD_PTR)&msg) == 0 &&
                (msg.hwnd != hwnd || TranslateAcceleratorW(hwnd, accel, &msg) == 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        } else if (PeekMessageW(&msg, 0, 0, 0, 0) != 0) {
            if (GetMessageW(&msg, 0, 0, 0) == 0) {
                break;
            }
            if (HtmlHelpW(0, 0, HH_PRETRANSLATEMESSAGE, (DWORD_PTR)&msg) == 0 &&
                (msg.hwnd != hwnd || TranslateAcceleratorW(hwnd, accel, &msg) == 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        } else if (g_game.iconic == 0 && g_game.active != 0) {
            g_game.anim->paint();
        } else {
            WaitMessage();
        }
    }
    return (int)msg.wParam;
}
