#include "game_api.h"

void RecBank::sort_desc()
{
    int i;
    int j;
    Rec24 tmp;

    for (i = 1; i < count; i++) {
        for (j = i; j > 0; j--) {
            if (items[j].f <= items[j - 1].f) {
                break;
            }
            tmp = items[j - 1];
            items[j - 1] = items[j];
            items[j] = tmp;
        }
    }
}
