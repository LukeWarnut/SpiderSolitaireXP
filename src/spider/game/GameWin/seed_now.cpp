#include "game_api.h"
#include <time.h>

void GameWin::seed_now()
{
    new_game(time(0));
}
