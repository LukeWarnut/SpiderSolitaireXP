#include "game_api.h"

void *__fastcall stride_fill(void *p)
{
    ((StrideCall *)p)->apply(
        (char *)p,
        0x48,
        0x64,
        (void (__fastcall *)(void *))stride_keep);
    return p;
}
