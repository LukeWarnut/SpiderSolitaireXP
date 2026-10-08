#include "game_api.h"

int DealView::can_drop(int src_pile, int src_card, int dst_pile)
{
    int last;

    last = ((ColumnInner *)col)->slots[dst_pile] - 1;
    if (src_pile == dst_pile) {
        return 0;
    } else {
        if (((CardColumn *)col)->slot_empty(src_pile))
            return 0;
        if (((CardColumn *)col)->slot_empty(dst_pile))
            return 1;
        if (suit_of(dst_pile, last) == 0)
            return 0;
        if (suit_of(src_pile, src_card) == 0)
            return 0;
        return face_of(dst_pile, last) == face_of(src_pile, src_card) + 1;
    }
}
