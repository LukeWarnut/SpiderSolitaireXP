#include "game_api.h"

void GameBoard::reset_attrs()
{
    AttrTable *p;

    drop_attrs();
    p = new AttrTable(2);
    attrs = p;
}
