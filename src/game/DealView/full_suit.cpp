#include "game_api.h"

int DealView::full_suit(int pile)
{
    int count;
    int face;
    int i;
    struct {
        int rank;
        int step;
    } loc;

    count = ((ColumnInner *)col)->slots[pile];
    if (count < 0xd) {
        return 0;
    }
    loc.rank = rank_of(pile, count - 1);
    face = face_of(pile, count - 1);
    loc.step = 2;
    i = count - 2;
    while (loc.step <= 0xd) {
        if (suit_of(pile, i) == 0) {
            return 0;
        }
        if (face_of(pile, i) != face + 1) {
            return 0;
        }
        if (rank_of(pile, i) != loc.rank) {
            return 0;
        }
        face++;
        loc.step++;
        i--;
    }
    return 1;
}
