#include "game_api.h"

Vec3 *__stdcall vec_scale(Vec3 *out, float s, Vec3 *v)
{
    float x;
    float y;
    float z;

    z = v->z * s;
    y = v->y * s;
    x = v->x * s;
    out->x = x;
    out->y = y;
    out->z = z;
    return out;
}
