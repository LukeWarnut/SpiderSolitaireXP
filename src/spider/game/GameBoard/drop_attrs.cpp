#include "game_api.h"

void GameBoard::drop_attrs()
{
    AttrTable *p = attrs;
    if (p != 0) {
        p->release(1);
        attrs = 0;
    }
}
