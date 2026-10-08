#include "game_api.h"

void GameBoard::drop_board()
{
    TenListBoard *p = board;
    if (p != 0) {
        p->release(1);
        board = 0;
    }
}
