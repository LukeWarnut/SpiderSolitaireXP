#pragma once

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Gold scalar-deleting dtors and AttrTable::clear_buf call cdecl _free. */
#define heap_free free

#ifdef __cplusplus
}
#endif

