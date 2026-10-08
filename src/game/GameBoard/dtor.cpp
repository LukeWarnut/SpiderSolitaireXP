#include "game_api.h"

GameBoard::~GameBoard()
{
    int *p;

    drop_attrs();
    drop_board();
    p = &hwnd;
    if (*p != 0) {
        HtmlHelpW(0, 0, HH_CLOSE_ALL, 0);
        HtmlHelpW(0, 0, HH_UNINITIALIZE, *p);
    }
}
