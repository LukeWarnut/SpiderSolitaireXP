#include "game_api.h"

float __stdcall vec_length_sq(Vec3 *v)
{
    return v->x * v->x + v->y * v->y + v->z * v->z;
}
