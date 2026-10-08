#include "game_api.h"

void AnimState::stop()
{
    if (b != 0) {
        clear_draw();
    }
}
