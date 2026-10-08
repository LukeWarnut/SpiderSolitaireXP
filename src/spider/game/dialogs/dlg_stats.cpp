#include "game_api.h"

extern "C" GameWin *g_stats_game;
GameWin *g_stats_game;

#define G g_stats_game

struct TabItem {
    UINT mask;
    DWORD state;
    DWORD state_mask;
    LPWSTR text;
    int text_max;
    int image;
    LPARAM param;
};

extern "C" INT_PTR CALLBACK fn_010076A9(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    wchar_t buf[100];
    TabItem item;
    HWND tab;

    switch (msg) {
    case WM_NOTIFY:
        if (((NMHDR *)lp)->code != (UINT)-551)
            return TRUE;
        fn_010071F3(hwnd, G);
        return TRUE;
    case WM_INITDIALOG:
        G = (GameWin *)lp;
        tab = GetDlgItem(hwnd, 0x3f3);
        item.image = -1;
        item.mask = 3;
        GetDlgItemTextW(hwnd, 0x3f4, buf, 100);
        item.text = buf;
        if (SendMessageW(tab, 0x133e, 0, (LPARAM)&item) == -1)
            return 0x80004005;
        GetDlgItemTextW(hwnd, 0x3f5, buf, 100);
        item.text = buf;
        if (SendMessageW(tab, 0x133e, 1, (LPARAM)&item) == -1)
            return 0x80004005;
        GetDlgItemTextW(hwnd, 0x3f6, buf, 100);
        item.text = buf;
        if (SendMessageW(tab, 0x133e, 2, (LPARAM)&item) == -1)
            return 0x80004005;
        if (*G->mode == 1)
            SendMessageW(tab, 0x130c, 0, 0);
        else if (*G->mode == 2)
            SendMessageW(tab, 0x130c, 1, 0);
        else
            SendMessageW(tab, 0x130c, 2, 0);
        fn_010071F3(hwnd, G);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case 1:
        case 2:
            EndDialog(hwnd, 1);
            return TRUE;
        case 0x3ea:
            if (alert_box(hwnd, 0x34, 2, 0x124) != IDYES)
                return FALSE;
            if (G == 0)
                return TRUE;
            G->reset_suit_slots();
            fn_010071F3(hwnd, G);
            return TRUE;
        }
    }
    return FALSE;
}
