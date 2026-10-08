#include "game_api.h"

void GameWin::tally(int arg)
{
    int *row;
    int *p;
    int n;
    int found;
    int m;

    if (arg == 0) {
        if (in_play == 0) {
            if (f58 == 0) {
                return;
            }
        }
        found = 0;
        p = z;
        n = 4;
        do {
            if (*p < 2) {
                found = 1;
            }
            p++;
            n--;
        } while (n != 0);
        if (found == 0) {
            return;
        }
    }
    m = *mode;
    if (m == 1) {
        row = &suit_slot[0][0];
    } else if (m == 2) {
        row = &suit_slot[1][0];
    } else {
        row = &suit_slot[2][0];
    }
    if (arg != 0) {
        if (row[6] != 0) {
            row[5]++;
        } else {
            row[6] = 1;
            row[5] = 1;
        }
        if (row[5] > row[3]) {
            row[3] = row[5];
        }
    } else {
        if (row[6] != 0) {
            row[6] = 0;
            row[5] = 1;
        } else {
            row[5]++;
        }
        if (row[5] > row[4]) {
            row[4] = row[5];
        }
    }
    if (arg != 0) {
        row[1]++;
    } else {
        row[2]++;
    }
    save_settings();
}
