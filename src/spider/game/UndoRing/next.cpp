#include "game_api.h"

inline UndoRec *UndoRing::next()
{
    int i = idx;
    int n = i + 1;
    idx = n;
    if (n == limit) {
        idx = 0;
    }
    return &items[i];
}

/* inline COMDATs are omitted if unreferenced. External linkage keeps this
 * global, which forces the out-of-line copy this split unit must emit. */
UndoRec *(UndoRing::*k_next)() = &UndoRing::next;
