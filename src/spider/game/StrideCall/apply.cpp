#include "game_api.h"

void StrideCall::apply(char *p, int stride, int count, void (__fastcall *fn)(void *))
{
    int n;
    int i;
    char *s;

    if (--count < 0) {
        return;
    }
    n = count;
    s = p;
    i = n + 1;
    do {
        fn(s);
        s += stride;
    } while (--i);
}
