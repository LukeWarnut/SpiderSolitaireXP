#include "game_api.h"

extern "C" int __stdcall alert_box(HWND hwnd, UINT textId, UINT capId, UINT type)
{
    wchar_t cap[0x400];

    lstrcpyW(cap, load_string(capId));
    return MessageBoxW(hwnd, load_string(textId), cap, type);
}
