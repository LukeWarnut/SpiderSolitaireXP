#include "game_api.h"
#include <stdlib.h>
#include <mmsystem.h>

inline Vec3 *vset(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
    return v;
}

void AnimState::burst_fx(FxBank *fx)
{
    Vec3 base;
    float speed;
    int side;
    int mid;
    int end;
    FxItem *it;
    int n;
    Vec3 pos;
    Vec3 vel;
    Vec3 dir;
    Vec3 nrm;
    Vec3 scaled;
    Vec3 acc;
    float t;

    base.x = ((float)rand() - (float)rand()) * 3.051851e-05f;
    base.x = base.x * 20.0f;
    base.y = 30.0f;
    base.z = 0.0f;
    speed = (float)rand() * 3.051851e-05f;
    speed = speed * 10.0f + 20.0f;
    fx->stamp = (float)(DWORD)timeGetTime() * 0.001f;
    side = rand() % 2;
    mid = rand() % 2;
    end = rand() % 2;
    if (side == 0 && mid == 0) {
        end = 1;
    }
    fx->gx = side ? -2.0f : -150.0f;
    fx->gy = mid ? -2.0f : -150.0f;
    fx->gz = end ? -2.0f : -150.0f;
    it = fx->items;
    n = 100;
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    vel.x = 0.0f;
    vel.y = 0.0f;
    vel.z = 0.0f;
    do {
        *(Vec3 *)&it->x = pos;
        it->vel0 = base;
        it->vel = vel;
        vset(&dir, ((float)rand() - (float)rand()) * 3.051851e-05f,
             ((float)rand() - (float)rand()) * 3.051851e-05f,
             ((float)rand() - (float)rand()) * 3.051851e-05f);
        it->acc = *vec_add(&acc, &base, vec_scale(&scaled, speed, vec_normalize(&nrm, &dir)));
        it->r = it->g = it->b = 0.1f;
        t = ((float)rand() - (float)rand()) * 3.051851e-05f;
        it->scale = 0.2f;
        t = t * 0.125f;
        it->pad3c = (t + 1.0f) * 4.0f;
        it->life = 0.0f;
        it++;
    } while (--n);
}
