#include "game_api.h"

int RecBank::push(Rec24 *rec)
{
    if (count < 0x1f) {
        items[count].a = rec->a;
        items[count].b = rec->b;
        items[count].c = rec->c;
        items[count].d = rec->d;
        items[count].e = rec->e;
        items[count].f = rec->f;
        count++;
        return count;
    }
    return -1;
}
