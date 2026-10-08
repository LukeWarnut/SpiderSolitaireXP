#include "game_api.h"

int DealView::run_ok(int pile, int card)
{
    int n;
    int last;
    int rank;
    int face;

    n = ((ColumnInner *)col)->slots[pile];
    if (((CardColumn *)col)->slot_empty(pile)) {
        return 0;
    }
    if (suit_of(pile, card) == 0) {
        return 0;
    }
    last = n - 1;
    if (card >= last) {
        return 1;
    }
    rank = rank_of(pile, card);
    face = face_of(pile, card);
    for (card++; card <= last; card++) {
        if (rank_of(pile, card) != rank) {
            return 0;
        }
        if (face_of(pile, card) != face - 1) {
            return 0;
        }
        face--;
    }
    return 1;
}
