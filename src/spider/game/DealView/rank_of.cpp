#include "game_api.h"

int DealView::rank_of(int pile, int card)
{
    PileTable *p;
    TripleTable *t;

    if (((CardColumn *)col)->slot_empty(pile)) {
        return -1;
    }
    p = (PileTable *)col;
    t = triples;
    return t->rank(p->card_value(pile, card));
}
