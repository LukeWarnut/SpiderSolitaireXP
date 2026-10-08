#include "game_api.h"

void PileTable::trim_pile(int pile, int keep)
{
    int *countp = &counts[pile];
    int i = *countp - 1;
    CardList **slot;

    if (i >= keep) {
        slot = &piles[pile];
        do {
            (*slot)->unlink(i);
            i--;
        } while (i >= keep);
    }
    *countp = keep;
}
