#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI cdtInit(int *pdx, int *pdy);
void cdtTerm(void);
BOOL WINAPI cdtDraw(HDC hdc, int x, int y, int card, int type, DWORD color);
BOOL WINAPI cdtDrawExt(HDC hdc, int x, int y, int dx, int dy, int card, int type, DWORD color);
BOOL WINAPI cdtAnimate(HDC hdc, int card, int x, int y, int frame);
int WINAPI WEP(int);

#ifdef __cplusplus
}
#endif
