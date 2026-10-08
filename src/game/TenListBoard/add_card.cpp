#include "game_api.h"

void TenListBoard::add_card(int pile, int value, int hidden)
{
    CardList *list;
    int i;
    int buf[3];

    i = pile;
    list = lists[i];
    if (list == 0) {
        return;
    }
    buf[0] = value;
    if (list->append(&buf[0], &pile) == 0) {
        return;
    }
    counts[i]++;
    if (hidden == 0) {
        extras[i]++;
    }
}
