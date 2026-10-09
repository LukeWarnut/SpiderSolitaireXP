#include "cards.h"

static int g_face_dy;
static int g_face_dx;
static HBITMAP g_draw_bmp;
static int g_n_loaded;
static HBITMAP g_face_bmps[52];
static HBITMAP g_hbmH;
static HBITMAP g_hbmCover;
static HBITMAP g_hbmX;
static HBITMAP g_hbmNaught;
static int g_anim_id;
static int g_init_cnt;
static int g_lru_pos;
static HINSTANCE g_hinst_dll;

void WINAPI save_corners(HDC hdc, COLORREF *c, int x, int y, int dx, int dy);
void WINAPI restore_corners(HDC hdc, COLORREF *c, int x, int y, int dx, int dy);
static HBITMAP load_face(int card);
static void WINAPI delete_if(HGDIOBJ obj);
BOOL WINAPI load_back(int id);

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    g_hinst_dll = inst;
    return TRUE;
}

void WINAPI save_corners(HDC hdc, COLORREF *c, int x, int y, int dx, int dy)
{
    if (dx != g_face_dx)
        return;
    if (dy != g_face_dy)
        return;
    c[0] = GetPixel(hdc, x, y);
    c[1] = GetPixel(hdc, x + 1, y);
    c[2] = GetPixel(hdc, x, y + 1);
    x = x + dx - 1;
    c[3] = GetPixel(hdc, x, y);
    c[4] = GetPixel(hdc, x - 1, y);
    c[5] = GetPixel(hdc, x, y + 1);
    y = y + dy - 1;
    c[6] = GetPixel(hdc, x, y);
    dy = y - 1;
    c[7] = GetPixel(hdc, x, dy);
    c[8] = GetPixel(hdc, x - 1, y);
    x += 1 - dx;
    c[9] = GetPixel(hdc, x, y);
    c[10] = GetPixel(hdc, x + 1, y);
    c[11] = GetPixel(hdc, x, dy);
}

BOOL WINAPI cdtAnimate(HDC hdc, int card, int x, int y, int frame)
{
    return TRUE;
}

void WINAPI restore_corners(HDC hdc, COLORREF *c, int x, int y, int dx, int dy)
{
    if (dx != g_face_dx)
        return;
    if (dy != g_face_dy)
        return;
    SetPixel(hdc, x, y, c[0]);
    SetPixel(hdc, x + 1, y, c[1]);
    SetPixel(hdc, x, y + 1, c[2]);
    x = x + dx - 1;
    SetPixel(hdc, x, y, c[3]);
    SetPixel(hdc, x - 1, y, c[4]);
    SetPixel(hdc, x, y + 1, c[5]);
    y = y + dy - 1;
    SetPixel(hdc, x, y, c[6]);
    dy = y - 1;
    SetPixel(hdc, x, dy, c[7]);
    SetPixel(hdc, x - 1, y, c[8]);
    x += 1 - dx;
    SetPixel(hdc, x, y, c[9]);
    SetPixel(hdc, x + 1, y, c[10]);
    SetPixel(hdc, x, dy, c[11]);
}

static HBITMAP load_face(int card)
{
    int id;

    if (!g_face_bmps[card]) {
        if (g_n_loaded >= 5) {
            while (!g_face_bmps[g_lru_pos])
                g_lru_pos = (g_lru_pos == 51) ? 0 : g_lru_pos + 1;
            DeleteObject(g_face_bmps[g_lru_pos]);
            g_n_loaded--;
            g_face_bmps[g_lru_pos] = 0;
        }
        id = (card >> 2) % 13 + (card & 3) * 13;
        while (!(g_face_bmps[card] = LoadBitmapA(g_hinst_dll, MAKEINTRESOURCEA(id + 1)))) {
            if (!g_n_loaded)
                return 0;
            while (!g_face_bmps[g_lru_pos])
                g_lru_pos = (g_lru_pos == 51) ? 0 : g_lru_pos + 1;
            DeleteObject(g_face_bmps[g_lru_pos]);
            g_face_bmps[g_lru_pos] = 0;
            g_n_loaded--;
        }
        g_n_loaded++;
    }
    return g_face_bmps[card];
}

static void WINAPI delete_if(HGDIOBJ obj)
{
    if (obj)
        DeleteObject(obj);
}

void cdtTerm(void)
{
    int i;

    g_init_cnt--;
    if (g_init_cnt > 0)
        return;
    for (i = 0; i < 52; i++)
        delete_if(g_face_bmps[i]);
    delete_if(g_hbmH);
    delete_if(g_hbmCover);
    delete_if(g_hbmX);
    delete_if(g_hbmNaught);
}

int WINAPI WEP(int unused)
{
    return TRUE;
}

BOOL WINAPI cdtInit(int *pdx, int *pdy)
{
    BITMAP bm;

    if (g_init_cnt++) {
        *pdx = g_face_dx;
        *pdy = g_face_dy;
        return TRUE;
    }
    g_hbmH = LoadBitmapA(g_hinst_dll, MAKEINTRESOURCEA(53));
    g_hbmX = LoadBitmapA(g_hinst_dll, MAKEINTRESOURCEA(67));
    g_hbmNaught = LoadBitmapA(g_hinst_dll, MAKEINTRESOURCEA(68));
    if (!g_hbmH || !g_hbmX || !g_hbmNaught) {
        delete_if(g_hbmH);
        delete_if(g_hbmX);
        delete_if(g_hbmNaught);
        return FALSE;
    }
    GetObjectA(g_hbmH, sizeof(bm), &bm);
    *pdx = bm.bmWidth;
    g_face_dx = bm.bmWidth;
    *pdy = bm.bmHeight;
    g_face_dy = bm.bmHeight;
    return TRUE;
}

BOOL WINAPI load_back(int id)
{
    int cur;

    cur = g_anim_id;
    if (cur != id) {
        delete_if(g_hbmCover);
        g_hbmCover = LoadBitmapA(g_hinst_dll, MAKEINTRESOURCEA((WORD)id));
        cur = g_hbmCover ? id : 0;
        g_anim_id = cur;
    }
    return cur != 0;
}

BOOL WINAPI cdtDrawExt(HDC hdc, int x, int y, int dx, int dy, int card, int type, DWORD color)
{
    HDC mem;
    HGDIOBJ brush;
    POINT org;
    COLORREF corners[12];
    DWORD rop;
    int ghost;
    int face;

    rop = 0;
    ghost = 0;
    if (type & 0x80000000) {
        type -= 0x80000000;
        ghost = 1;
    }
    if ((unsigned)type <= 7) {
        switch (type) {
        case 0:
            g_draw_bmp = load_face(card);
            rop = SRCCOPY;
            color = 0xFFFFFF;
            break;
        case 1:
            if (!load_back(card)) {
                return FALSE;
            } else {
                g_draw_bmp = g_hbmCover;
                rop = SRCCOPY;
            }
            break;
        case 3:
        case 4:
            brush = CreateSolidBrush(color);
            if (!brush) {
                return FALSE;
            } else {
                GetDCOrgEx(hdc, &org);
                SetBrushOrgEx(hdc, org.x, org.y, 0);
                brush = SelectObject(hdc, brush);
                if (brush) {
                    PatBlt(hdc, x, y, dx, dy, PATCOPY);
                    brush = SelectObject(hdc, brush);
                    if (brush)
                        DeleteObject(brush);
                }
                if (type == 4)
                    return TRUE;
                g_draw_bmp = g_hbmH;
                rop = SRCAND;
            }
            break;
        case 5:
            g_draw_bmp = g_hbmH;
            rop = SRCAND;
            break;
        case 6:
            g_draw_bmp = g_hbmX;
            rop = SRCCOPY;
            break;
        case 7:
            g_draw_bmp = g_hbmNaught;
            rop = SRCCOPY;
            break;
        case 2:
            g_draw_bmp = load_face(card);
            rop = NOTSRCCOPY;
            break;
        }
    }
    if (!g_draw_bmp) {
        return FALSE;
    } else {
    mem = CreateCompatibleDC(hdc);
    if (!mem) {
        return FALSE;
    } else {
    g_draw_bmp = SelectObject(mem, g_draw_bmp);
    if (g_draw_bmp) {
        color = SetBkColor(hdc, color);
        if (!ghost)
            save_corners(hdc, corners, x, y, dx, dy);
        if (dx == g_face_dx && dy == g_face_dy)
            BitBlt(hdc, x, y, g_face_dx, g_face_dy, mem, 0, 0, rop);
        else
            StretchBlt(hdc, x, y, dx, dy, mem, 0, 0, g_face_dx, g_face_dy, rop);
        SelectObject(mem, g_draw_bmp);
        if (type == 0) {
            face = (card >> 2) % 13 + (card & 3) * 13 + 1;
            if ((face >= 14 && face <= 23) || (face >= 27 && face <= 36)) {
                PatBlt(hdc, x + 2, y, dx - 4, 1, BLACKNESS);
                PatBlt(hdc, x + dx - 1, y + 2, 1, dy - 4, BLACKNESS);
                PatBlt(hdc, x + 2, y + dy - 1, dx - 4, 1, BLACKNESS);
                PatBlt(hdc, x, y + 2, 1, dy - 4, BLACKNESS);
                SetPixel(hdc, x + 1, y + 1, 0);
                SetPixel(hdc, x + dx - 2, y + 1, 0);
                SetPixel(hdc, x + dx - 2, y + dy - 2, 0);
                SetPixel(hdc, x + 1, y + dy - 2, 0);
            }
        }
        if (!ghost)
            restore_corners(hdc, corners, x, y, dx, dy);
        SetBkColor(hdc, color);
    }
    DeleteDC(mem);
    return TRUE;
    }
    }
}

BOOL WINAPI cdtDraw(HDC hdc, int x, int y, int card, int type, DWORD color)
{
    return cdtDrawExt(hdc, x, y, g_face_dx, g_face_dy, card, type, color);
}
