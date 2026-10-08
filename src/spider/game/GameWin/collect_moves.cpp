#include "game_api.h"

void GameWin::collect_moves()
{
    DealView *view;
    int src;
    int dst;
    int last;
    int prev;
    int kept;
    int dst_last;
    int kind;
    Rec24 rec;

    view = (DealView *)this;
    ((RecBank *)this)->count = 0;
    ((RecBank *)this)->pad2 = 0;
    for (src = 0; src < 10; src++) {
        for (dst = 0; dst < 10; dst++) {
            if (src != dst) {
                last = ((ColumnInner *)view->col)->slots[src] - 1;
                kept = last;
                if (last > 0) {
                    prev = last - 1;
                    do {
                        int fa;
                        int fb;

                        kept = last;
                        fa = view->rank_of(src, last);
                        fb = view->rank_of(src, prev);
                        if (fa != fb) {
                            break;
                        }
                        if (view->face_of(src, last) != view->face_of(src, prev) - 1) {
                            break;
                        }
                        if (view->suit_of(src, prev) == 0) {
                            break;
                        }
                        last--;
                        prev--;
                        kept = last;
                    } while (last > 0);
                }
                if (view->can_drop(src, last, dst) != 0) {
                    dst_last = ((ColumnInner *)view->col)->slots[dst] - 1;
                    kind = view->rank_kind(src, last, dst, dst_last);
                    if (kind > 0) {
                        rec.a = view->rank_of(src, last) == view->rank_of(dst, dst_last);
                        rec.b = src;
                        rec.c = kept;
                        rec.d = dst;
                        rec.e = dst_last;
                        rec.f = kind;
                        ((RecBank *)this)->push(&rec);
                    }
                }
            }
        }
    }
    *(int *)((char *)this + 0x5c) = 1;
}
