#include "game_api.h"
#include <stdlib.h>

void TripleTable::shuffle(int seed)
{
    int n;
    int *bits;
    int pass;
    int suit;
    int slot;
    int rem;
    int *row;
    int i;
    int *hit;

    n = limit;
    bits = (int *)::operator new(n * 4);
    if (bits == 0) {
        return;
    }
    srand(seed);
    for (i = 0; i < n; i++) {
        bits[i] = 0;
    }
    pass = 0;
    if (decks > 0) {
        do {
            suit = 0;
            do {
                slot = 0;
                do {
                    do {
                        rem = rand() % n;
                        hit = &bits[rem];
                    } while (*hit != 0);
                    *hit = 1;
                    row = ptr + rem * 3;
                    row[0] = suit;
                    if (mode == 1) {
                        row[0] = 3;
                    } else if (mode == 2) {
                        if (suit == 0) {
                            row[0] = 3;
                        }
                        if (row[0] == 1) {
                            row[0] = 2;
                        }
                    }
                    row[1] = slot;
                    row[2] = 0;
                    slot++;
                } while (slot <= 0xc);
                suit++;
            } while (suit <= 3);
            pass++;
        } while (pass < decks);
    }
    ::operator delete(bits);
    idx = 0;
}
