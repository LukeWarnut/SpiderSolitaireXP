#include "game_api.h"

void GameWin::save_settings()
{
    WINDOWPLACEMENT wp;
    RECT rc;
    DWORD show;
    DWORD v;
    DWORD opt;
    HKEY key;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, load_string(9), 0, 0, 0, KEY_WRITE, 0, &key, &v) != ERROR_SUCCESS)
        return;
    show = IsZoomed(hwnd) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    if (!IsZoomed(hwnd) && !IsIconic(hwnd)) {
        GetWindowRect(hwnd, &rc);
    } else {
        wp.length = sizeof(wp);
        GetWindowPlacement(hwnd, &wp);
        rc = wp.rcNormalPosition;
    }
    RegSetValueExW(key, load_string(0xa), 0, REG_DWORD, (BYTE *)&show, 4);
    RegSetValueExW(key, load_string(0xb), 0, REG_DWORD, (BYTE *)&rc.left, 4);
    RegSetValueExW(key, load_string(0xc), 0, REG_DWORD, (BYTE *)&rc.top, 4);
    v = rc.right - rc.left;
    RegSetValueExW(key, load_string(0xd), 0, REG_DWORD, (BYTE *)&v, 4);
    v = rc.bottom - rc.top;
    RegSetValueExW(key, load_string(0xe), 0, REG_DWORD, (BYTE *)&v, 4);
    opt = use_fx;
    RegSetValueExW(key, load_string(0x15), 0, REG_DWORD, (BYTE *)&opt, 4);
    opt = opt_f64;
    RegSetValueExW(key, load_string(0x16), 0, REG_DWORD, (BYTE *)&opt, 4);
    opt = opt_f68;
    RegSetValueExW(key, load_string(0x17), 0, REG_DWORD, (BYTE *)&opt, 4);
    opt = opt_f6c;
    RegSetValueExW(key, load_string(0x18), 0, REG_DWORD, (BYTE *)&opt, 4);
    opt = opt_f70;
    RegSetValueExW(key, load_string(0x19), 0, REG_DWORD, (BYTE *)&opt, 4);
    opt = play_snd;
    RegSetValueExW(key, load_string(0x33), 0, REG_DWORD, (BYTE *)&opt, 4);
    v = *mode;
    RegSetValueExW(key, load_string(0x31), 0, REG_DWORD, (BYTE *)&v, 4);
    RegSetValueExW(key, L"HighScore_Easy", 0, REG_DWORD, (BYTE *)&suit_slot[0][0], 4);
    RegSetValueExW(key, L"Wins_Easy", 0, REG_DWORD, (BYTE *)&suit_slot[0][1], 4);
    RegSetValueExW(key, L"Losses_Easy", 0, REG_DWORD, (BYTE *)&suit_slot[0][2], 4);
    RegSetValueExW(key, L"StreakWins_Easy", 0, REG_DWORD, (BYTE *)&suit_slot[0][3], 4);
    RegSetValueExW(key, L"StreakLosses_Easy", 0, REG_DWORD, (BYTE *)&suit_slot[0][4], 4);
    RegSetValueExW(key, L"StreakCurrent_Easy", 0, REG_DWORD, (BYTE *)&suit_slot[0][5], 4);
    RegSetValueExW(key, L"FWinStreak_Easy", 0, REG_DWORD, (BYTE *)&suit_slot[0][6], 4);
    RegSetValueExW(key, L"HighScore_Medium", 0, REG_DWORD, (BYTE *)&suit_slot[1][0], 4);
    RegSetValueExW(key, L"Wins_Medium", 0, REG_DWORD, (BYTE *)&suit_slot[1][1], 4);
    RegSetValueExW(key, L"Losses_Medium", 0, REG_DWORD, (BYTE *)&suit_slot[1][2], 4);
    RegSetValueExW(key, L"StreakWins_Medium", 0, REG_DWORD, (BYTE *)&suit_slot[1][3], 4);
    RegSetValueExW(key, L"StreakLosses_Medium", 0, REG_DWORD, (BYTE *)&suit_slot[1][4], 4);
    RegSetValueExW(key, L"StreakCurrent_Medium", 0, REG_DWORD, (BYTE *)&suit_slot[1][5], 4);
    RegSetValueExW(key, L"FWinStreak_Medium", 0, REG_DWORD, (BYTE *)&suit_slot[1][6], 4);
    RegSetValueExW(key, L"HighScore_Difficult", 0, REG_DWORD, (BYTE *)&suit_slot[2][0], 4);
    RegSetValueExW(key, L"Wins_Difficult", 0, REG_DWORD, (BYTE *)&suit_slot[2][1], 4);
    RegSetValueExW(key, L"Losses_Difficult", 0, REG_DWORD, (BYTE *)&suit_slot[2][2], 4);
    RegSetValueExW(key, L"StreakWins_Difficult", 0, REG_DWORD, (BYTE *)&suit_slot[2][3], 4);
    RegSetValueExW(key, L"StreakLosses_Difficult", 0, REG_DWORD, (BYTE *)&suit_slot[2][4], 4);
    RegSetValueExW(key, L"StreakCurrent_Difficult", 0, REG_DWORD, (BYTE *)&suit_slot[2][5], 4);
    RegSetValueExW(key, L"FWinStreak_Difficult", 0, REG_DWORD, (BYTE *)&suit_slot[2][6], 4);
    RegCloseKey(key);
}
