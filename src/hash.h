/*
 * MIT License
 * Copyright (c) 2025 IMSDcrueoft (https://github.com/IMSDcrueoft)
 * See LICENSE file in the root directory for full license text.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#define HASH_64bits(str,len) MurmurHash64Bits(str, len)
uint64_t MurmurHash64Bits(const void* key, size_t len);
