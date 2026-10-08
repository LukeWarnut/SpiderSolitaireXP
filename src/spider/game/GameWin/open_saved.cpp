#include "game_api.h"
#include <shlobj.h>

HANDLE GameWin::open_saved(DWORD access, DWORD creation)
{
    wchar_t path[0x106];
    wchar_t *(WINAPI *esi)(wchar_t *, LPCWSTR);

    SHGetSpecialFolderPathW(hwnd, path, 5, 1);
    esi = lstrcatW;
    esi(path, L"\\");
    esi(path, load_string(0x14));
    return CreateFileW(
        path,
        access,
        0,
        0,
        creation,
        FILE_FLAG_SEQUENTIAL_SCAN,
        0);
}
