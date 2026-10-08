#include "game_api.h"

void GameBoard::reset_board()
{
    TenListBoard *p;

    drop_board();
    p = new TenListBoard;
    board = p;
}
