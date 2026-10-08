#include "game_api.h"
#include <stdlib.h>
#include <string.h>

#undef WinMain

/*
 * Runtime harness. Not part of any matched unit.
 *
 * Game globals (g_board, g_hinst, ...) are defined by the matched units and
 * live in this image's .data. Only the SPIDER_SHOT comparison runs gold
 * code: the runnable link (build/XPSP1/run/spider.exe, /BASE:0x00400000)
 * leaves 0x01000000 free, so spider_boot maps the original image there and
 * resolves its import table, and shot_thread copies our board into gold's
 * before calling gold paint_hdc. The matching link (/BASE:0x01000000) cannot
 * map it and skips the gold render.
 */

static unsigned char *g_gold;

static void map_gold_image(void)
{
    HANDLE f;
    DWORD size;
    DWORD got;
    unsigned char *file;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    unsigned char *base;
    int i;

    f = CreateFileW(L"orig\\XPSP1\\spider.exe", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (f == INVALID_HANDLE_VALUE)
        return;
    size = GetFileSize(f, 0);
    file = (unsigned char *)malloc(size);
    ReadFile(f, file, size, &got, 0);
    CloseHandle(f);
    nt = (IMAGE_NT_HEADERS *)(file + ((IMAGE_DOS_HEADER *)file)->e_lfanew);
    base = (unsigned char *)VirtualAlloc((void *)nt->OptionalHeader.ImageBase, nt->OptionalHeader.SizeOfImage,
                                         MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (base == 0) {
        free(file);
        return;
    }
    memcpy(base, file, nt->OptionalHeader.SizeOfHeaders);
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        DWORD n = sec[i].SizeOfRawData;

        if (n > sec[i].Misc.VirtualSize)
            n = sec[i].Misc.VirtualSize;
        memcpy(base + sec[i].VirtualAddress, file + sec[i].PointerToRawData, n);
    }
    free(file);
    nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    imp = (IMAGE_IMPORT_DESCRIPTOR *)(base +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imp->Name != 0; imp++) {
        HMODULE dll = LoadLibraryA((char *)(base + imp->Name));
        DWORD *names = (DWORD *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        DWORD *iat = (DWORD *)(base + imp->FirstThunk);

        for (; *names != 0; names++, iat++) {
            if (*names & IMAGE_ORDINAL_FLAG32)
                *iat = (DWORD)GetProcAddress(dll, (char *)(*names & 0xffff));
            else
                *iat = (DWORD)GetProcAddress(dll, (char *)(base + *names + 2));
        }
    }
    g_gold = base;
}

extern "C" void spider_boot(void)
{
    map_gold_image();
}

static void save_bmp(HDC dc, HBITMAP bmp, int w, int h, const char *path)
{
    BITMAPINFOHEADER bi;
    BITMAPFILEHEADER bf;
    int stride;
    unsigned char *bits;
    HANDLE f;
    DWORD put;

    memset(&bi, 0, sizeof(bi));
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    stride = (w * 3 + 3) & ~3;
    bits = (unsigned char *)malloc(stride * h);
    GetDIBits(dc, bmp, 0, h, bits, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
    memset(&bf, 0, sizeof(bf));
    bf.bfType = 0x4d42;
    bf.bfOffBits = sizeof(bf) + sizeof(bi);
    bf.bfSize = bf.bfOffBits + stride * h;
    f = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    WriteFile(f, &bf, sizeof(bf), &put, 0);
    WriteFile(f, &bi, sizeof(bi), &put, 0);
    WriteFile(f, bits, stride * h, &put, 0);
    CloseHandle(f);
    free(bits);
}

typedef void (GameWin::*PaintFn)(HDC);

static void render(GameWin *game, PaintFn fn, int w, int h, const char *path)
{
    HDC screen = GetDC(0);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ old = SelectObject(dc, bmp);

    PatBlt(dc, 0, 0, w, h, BLACKNESS);
    (game->*fn)(dc);
    SelectObject(dc, old);
    save_bmp(dc, bmp, w, h, path);
    DeleteObject(bmp);
    DeleteDC(dc);
    ReleaseDC(0, screen);
}

static BOOL CALLBACK accept_dialog(HWND hwnd, LPARAM lp)
{
    char cls[32];
    DWORD pid;

    (void)lp;
    GetWindowThreadProcessId(hwnd, &pid);
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (pid == GetCurrentProcessId() && lstrcmpA(cls, "#32770") == 0)
        PostMessageA(hwnd, WM_COMMAND, IDOK, 0);
    return TRUE;
}

static DWORD WINAPI shot_thread(void *arg)
{
    char path[MAX_PATH];
    const char *prefix = (const char *)arg;
    RECT rc;
    union {
        PaintFn m;
        void *p;
    } gold;

    Sleep(2500);
    EnumWindows(accept_dialog, 0);
    Sleep(8000);
    GetClientRect(g_game.hwnd, &rc);
    wsprintfA(path, "%s_ours.bmp", prefix);
    render(&g_game, &GameWin::paint_hdc, rc.right, rc.bottom, path);
    if (g_gold != 0) {
        gold.m = 0;
        gold.p = (void *)0x0100598e;
        memcpy(g_gold + 0x10fc8, &g_board, sizeof(g_board));
        *(HINSTANCE *)(g_gold + 0x10fc0) = g_hinst;
        wsprintfA(path, "%s_gold.bmp", prefix);
        render((GameWin *)(g_gold + 0x10fc8), gold.m, rc.right, rc.bottom, path);
    }
    ExitProcess(0);
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd, int show)
{
    static char prefix[MAX_PATH];

    spider_boot();
    if (GetEnvironmentVariableA("SPIDER_SHOT", prefix, sizeof(prefix)) != 0)
        CloseHandle(CreateThread(0, 0, shot_thread, prefix, 0, 0));
    return fn_01006CED(instance, prev, cmd, show);
}

