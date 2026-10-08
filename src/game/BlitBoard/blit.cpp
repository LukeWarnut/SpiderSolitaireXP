#include "game_api.h"

void BlitBoard::blit()
{
    dst = src;
    BitBlt(hdcDst, 0, 0, src.c, src.d, hdcSrc, 0, 0, SRCCOPY);
}
