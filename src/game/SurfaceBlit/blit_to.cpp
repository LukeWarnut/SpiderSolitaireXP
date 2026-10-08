#include "game_api.h"

void SurfaceBlit::blit_to(HDC hdc)
{
    BitBlt(hdc, 0, 0, cx, cy, hdcSrc, 0, 0, SRCCOPY);
}
