#include "game_api.h"

int DealView::card_code(int pile, int index)
{
    int value;

    if (index == -1) {
        return 0x6c;
    }
    value = ((PileTable *)col)->card_value(pile, index);
    if (triples->suit(value) != 0) {
        TripleTable *table;

        table = triples;
        return table->rank(value) * 13 + table->face(value) + 1;
    }
    return 0x68;
}
