#include "game_api.h"

inline int LayoutBox::card_x(int i)
{
    return (a + 0x47) * 9 - i * 12 + b;
}

/* inline COMDATs are omitted if unreferenced. External linkage keeps this
 * global, which forces the out-of-line copy this split unit must emit. */
int (LayoutBox::*k_card_x)(int) = &LayoutBox::card_x;
