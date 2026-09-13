/*
* MIT License
* Copyright (c) 2025 IMSDcrueoft (https://github.com/IMSDcrueoft)
* See LICENSE file in the root directory for full license text.
*/
#pragma once

// cloxBooster uses the standard library allocator. The only third-party
// dependency is the bundled slabAllocator (third-party/slabAllocator) used to
// speed up fixed-size object allocation.
#include <stdlib.h>

#include "slab.h"

#define mem_alloc malloc
#define mem_realloc realloc
#define mem_free free
#define mem_print_stats

// ---- slab layer ------------------------------------------------------------
// Fixed-size objects (see SLAB_OBJ_TYPES in object.h) are allocated from
// per-type slab caches instead of the raw heap. The caches are created
// lazily on first use and torn down by freeSlabCaches() at shutdown.

void* slab_allocObject(unsigned int objType, size_t size);
void slab_freeObject(unsigned int objType, void* pointer);

//destroy every slab cache, units are already gone at this point
void slab_freeCaches();
