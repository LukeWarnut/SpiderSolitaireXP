#include "game_api.h"

int DealView::face_of(int pile, int card)
{
    PileTable *p;
    TripleTable *t;

    if (((CardColumn *)col)->slot_empty(pile)) {
        return -1;
    }
    p = (PileTable *)col;
    t = triples;
    return t->face(p->card_value(pile, card));
}
