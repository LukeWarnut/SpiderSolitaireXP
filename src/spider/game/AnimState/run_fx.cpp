#include "game_api.h"
#include <stdlib.h>
#include <math.h>
#include <mmsystem.h>

#pragma intrinsic(exp)

inline Vec3 *vset(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
    return v;
}

void AnimState::run_fx(FxBank *fx)
{
    FxItem *it;
    int n;
    float age;
    float s;
    float f;
    float life;
    float now;
    Vec3 *r;

    now = (float)timeGetTime() * 0.001f;
    age = now - fx->stamp - 1.0f;
    fx->ready = 0;
    it = fx->items;
    n = 100;
    do {
        if (age < 0.0f) {
            Vec3 v0;
            Vec3 v;
            Vec3 tmp;
            Vec3 d0;
            Vec3 e0;

            v0 = it->vel0;
            v = it->vel;
            s = age - (((float)rand() - (float)rand()) * 3.051851e-05f + 1.0f) * 0.05f;
            tmp.x = v0.x * s;
            tmp.y = v0.y * s;
            tmp.z = v0.z * s;
            *(Vec3 *)it = *vec_add(&e0, &v, vec_div(&d0, &tmp, 1.5f));
        } else {
            Vec3 acc;
            Vec3 v;
            Vec3 k;
            Vec3 at;
            Vec3 g;
            Vec3 d1;
            Vec3 e1;
            Vec3 d2;
            /* Memory homes: x87 spills x and z around the y-only 6.8 subtract. */
            volatile float kx;
            volatile float kz;

            acc = it->acc;
            v = it->vel;
            f = (1.0f - (float)exp(age * -1.8f)) * 0.308642f;
            vset(&k, acc.x * 1.8f, acc.y * 1.8f, acc.z * 1.8f);
            kz = k.z;
            kx = k.x;
            k.y = k.y - 6.8f;
            vset(&at, kx * f, k.y * f, kz * f);
            g.x = age * 0.0f;
            g.y = age * -6.8f;
            g.z = g.x;
            r = vec_add(&d2, vec_add(&e1, &v, vec_div(&d1, &g, 1.8f)), &at);
            *(Vec3 *)it = *r;
            it->life = age / it->pad3c;
            life = it->life;
            it->r = (float)exp(life * life * fx->gx);
            it->g = (float)exp(life * life * fx->gy);
            it->b = (float)exp(life * life * fx->gz);
            it->scale = (float)exp(life * life * -1.0f);
            if (it->life >= 1.0f) {
                fx->ready++;
            }
        }
        it++;
        n--;
    } while (n != 0);
}
