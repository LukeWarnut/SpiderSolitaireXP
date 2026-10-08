#include "game_api.h"

extern "C" LRESULT __stdcall wnd_proc_thunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return g_game.wnd_proc(hwnd, msg, wp, lp);
}
