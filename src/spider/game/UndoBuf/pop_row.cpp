#include "game_api.h"

void UndoBuf::pop_row()
{
    int i;
    int n;

    n = count;
    for (i = 0; i < n - 1; i++) {
        rows[i] = rows[i + 1];
    }
    count = n - 1;
}
