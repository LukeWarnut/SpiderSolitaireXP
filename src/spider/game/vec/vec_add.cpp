#include "game_api.h"

Vec3 *__stdcall vec_add(Vec3 *out, Vec3 *a, Vec3 *b)
{
    float x;
    float y;
    float z;

    z = a->z + b->z;
    y = a->y + b->y;
    x = a->x + b->x;
    out->x = x;
    out->y = y;
    out->z = z;
    return out;
}
