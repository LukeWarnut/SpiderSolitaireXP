#include "game_api.h"

AnimState *AnimState::init_draw(HWND hwnd)
{
    ((StrideCall *)this)->apply(
        (char *)this + 0x108,
        0x1c34,
        2,
        (void (__fastcall *)(void *))stride_fill);
    b = 0;
    this->hwnd = hwnd;
    return this;
}
