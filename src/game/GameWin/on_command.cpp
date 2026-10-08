#include "game_api.h"

LRESULT GameWin::on_command(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND self_hwnd;
    wchar_t help[260];

    if (anim->b != 0) {
        anim->clear_draw();
        UpdateWindow(hwnd);
    }
    self_hwnd = this->hwnd;
    if (GetForegroundWindow() != self_hwnd) {
        if (LOWORD(wp) == 0x9c44) {
            SendMessageW(GetForegroundWindow(), WM_CLOSE, 0, 0);
            return 0;
        }
        return DefWindowProcW(GetForegroundWindow(), msg, wp, lp);
    }
    GetMenu(hwnd);
    switch (LOWORD(wp)) {
    case 0x9c45:
        if ((in_play != 0 || f58 != 0) && suit_lock == 0) {
            HWND w = this->hwnd;
            if (alert_box(w, 3, 2, 0x124) != IDYES)
                break;
        }
        seed_now();
        break;
    case 0x9c46:
        if (in_play > 0 || f58 > 0) {
            HWND w = this->hwnd;
            if (alert_box(w, 4, 2, 0x124) == IDYES)
                new_game(seed);
        }
        break;
    case 0x9c4b:
        save_game();
        break;
    case 0x9c4c:
        load_game();
        break;
    case 0x9c4a:
        pop_undo();
        break;
    case 0x9c47:
    case 0x9c50:
        deal();
        break;
    case 0x9c4e:
        show_dialog_118();
        break;
    case 0x9c4f:
        show_dialog_117();
        break;
    case 0x9c51:
        deal_prompt();
        break;
    case 0x9c44:
        SendMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;
    case 0x9c4d:
        blink_move();
        break;
    case 0x9c43:
        lstrcpyW(help, load_string(0x2b));
        HtmlHelpW(GetDesktopWindow(), help, HH_DISPLAY_TOPIC, 0);
        break;
    case 0x9c42:
        show_dialog_107();
        break;
    }
    return 0;
}
