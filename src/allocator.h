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

#include "arena_slab.h"

#define mem_alloc malloc
#define mem_realloc realloc
#define mem_free free
#define mem_print_stats

// ---- slab layer ------------------------------------------------------------
// Fixed-size GC objects (upvalue/closure/bound method/instance) are allocated
// from the bundled arena slab (<= 256B size classes carved from one reserved
// segment). The segment is reserved once by slab_init() at vm startup and
// released by slab_shutdown() at teardown.

bool slab_init();
void slab_trim();
void slab_log_info();
void slab_shutdown();

void* slab_allocObject(size_t size);
void slab_freeObject(size_t size, void* pointer);

//raw primitives for the slab-routed array path (no gc bookkeeping here)
#define SLAB_MAX_ALLOC ((size_t)SLOT_SIZE_MAX)
//true when the pointer lives inside the slab segment
bool slab_owns(void* pointer);
