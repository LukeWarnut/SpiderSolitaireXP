#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev, LPWSTR cmd, int show);

#ifdef __cplusplus
}
#endif
