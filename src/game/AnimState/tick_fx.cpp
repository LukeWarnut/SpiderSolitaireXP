#include "game_api.h"
#include <mmsystem.h>

void AnimState::tick_fx(FxBank *fx)
{
    DWORD t;
    float now;
    int n;
    FxItem *item;

    t = timeGetTime();
    now = (float)t * 0.001f;
    if (now < fx->stamp) {
        return;
    }
    item = fx->items;
    n = 100;
    do {
        if (item->life < 1.0f) {
            step_fx(item);
        }
        item++;
        n--;
    } while (n != 0);
    if (fx->ready == 100) {
        fx->stamp = now;
        burst_fx(fx);
    }
}
