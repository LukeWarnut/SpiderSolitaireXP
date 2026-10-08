#include "game_api.h"

void GameWin::pop_undo()
{
    int n;
    int *rec;

    undo_n--;
    n = undo_n;
    rec = undo[n];
    if (rec == 0) {
        return;
    }
    if (rec[4] == 0) {
        move_run(rec[2], rec[3], rec[1], 0, rec[0]);
    }
    if (undo_n == 0) {
        disable_undo_menu();
    }
}
