#include "game_api.h"
#include <math.h>

#pragma intrinsic(sqrt)

Vec3 *__stdcall vec_normalize(Vec3 *out, Vec3 *v)
{
    vec_div(out, v, (float)sqrt(vec_length_sq(v)));
    return out;
}
