#include "game_api.h"

AttrTable::AttrTable(int n)
{
    int alloc;

    zero = 0;
    count = n;
    span = n * 0x34;
    alloc = span * 12;
    tag = 4;
    p = ::operator new(alloc);
}
