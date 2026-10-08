#include "game_api.h"

extern "C" DWORD g_help_ids[] = {
    0x3e9, 0x1388,
    0x3ea, 0x1389,
    0x3eb, 0x138a,
    0x3ec, 0x138b,
    0x3ed, 0x138c,
    0x3ee, 0x138d,
    0, 0,
};

extern "C" GameWin *g_opts;
GameWin *g_opts;

#define G g_opts

extern "C" INT_PTR CALLBACK fn_010073B6(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    wchar_t buf[0x104];

    switch (msg) {
    case WM_HELP:
        lstrcpyW(buf, load_string(0x2c));
        WinHelpW((HWND)((HELPINFO *)lp)->hItemHandle, buf, HELP_WM_HELP, (ULONG_PTR)g_help_ids);
        return TRUE;
    case WM_CONTEXTMENU:
        lstrcpyW(buf, load_string(0x2c));
        WinHelpW((HWND)wp, buf, HELP_CONTEXTMENU, (ULONG_PTR)g_help_ids);
        return TRUE;
    case WM_INITDIALOG:
        G = (GameWin *)lp;
        if (lp != 0) {
            CheckDlgButton(hwnd, 0x3e9, G->use_fx != 0);
            CheckDlgButton(hwnd, 0x3ea, G->opt_f64 != 0);
            CheckDlgButton(hwnd, 0x3eb, G->opt_f68 != 0);
            CheckDlgButton(hwnd, 0x3ec, G->opt_f6c != 0);
            CheckDlgButton(hwnd, 0x3ed, G->opt_f70 != 0);
            CheckDlgButton(hwnd, 0x3ee, G->play_snd != 0);
        }
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case 1:
            if (G != 0) {
                G->use_fx = IsDlgButtonChecked(hwnd, 0x3e9) == BST_CHECKED;
                G->opt_f64 = IsDlgButtonChecked(hwnd, 0x3ea) == BST_CHECKED;
                G->opt_f68 = IsDlgButtonChecked(hwnd, 0x3eb) == BST_CHECKED;
                G->opt_f6c = IsDlgButtonChecked(hwnd, 0x3ec) == BST_CHECKED;
                G->opt_f70 = IsDlgButtonChecked(hwnd, 0x3ed) == BST_CHECKED;
                G->play_snd = IsDlgButtonChecked(hwnd, 0x3ee) == BST_CHECKED;
            }
            EndDialog(hwnd, 1);
            return TRUE;
        case 2:
            EndDialog(hwnd, 2);
            return TRUE;
        }
    }
    return FALSE;
}
