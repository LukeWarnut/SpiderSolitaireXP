#include "game_api.h"

Vec3 *__stdcall vec_div(Vec3 *out, Vec3 *v, float s)
{
    float x;
    float y;
    float z;

    s = 1.0f / s;
    z = v->z * s;
    y = v->y * s;
    x = v->x * s;
    out->x = x;
    out->y = y;
    out->z = z;
    return out;
}
