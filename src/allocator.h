/*
* MIT License
* Copyright (c) 2025 IMSDcrueoft (https://github.com/IMSDcrueoft)
* See LICENSE file in the root directory for full license text.
*/
#pragma once

// cloxBooster uses the standard library allocator. There are no third-party
// dependencies; allocation goes straight through malloc/realloc/free.
#include <stdlib.h>

#define mem_alloc malloc
#define mem_realloc realloc
#define mem_free free
#define mem_print_stats
