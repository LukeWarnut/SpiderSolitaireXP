#include "game_api.h"

wchar_t g_strbuf[0x400];

extern "C" wchar_t *__stdcall load_string(UINT id)
{
    LoadStringW(g_hinst, id, g_strbuf, 0x400);
    return g_strbuf;
}
