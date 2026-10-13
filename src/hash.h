/*
 * MIT License
 * Copyright (c) 2025 IMSDcrueoft (https://github.com/IMSDcrueoft)
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#define HASH_64bits(str,len) MurmurHash64Bits(str, len)
uint64_t MurmurHash64Bits(const void* key, size_t len);

//the murmur math needs the total length twice (h = seed ^ len at the start,
//h ^= len before the final mix), so streaming must be told the total upfront;
//trailing bytes (<8) are buffered at their natural LSB shift positions,
//which mirrors the one-shot tail layout exactly
typedef struct {
	uint64_t h;        //rolling state, seeded with seed ^ totalLength
	uint64_t totalLen;
	uint64_t block;    //byte assembly buffer
	uint32_t buffered; //0..7
} HashStream;

//streaming hash: hashStreamBegin(totalLength) + N * hashStreamUpdate +
//hashStreamFinalize is bit-identical to HASH_64bits over the concatenated
//bytes (dedup pool consistency depends on this invariant)
void hashStreamBegin(HashStream* stream, uint64_t totalLength);
void hashStreamUpdate(HashStream* stream, const void* data, size_t length);
uint64_t hashStreamFinalize(HashStream* stream);

//debug self-check: streaming == one-shot over boundary sizes and every
//two-segment split; no-op under NDEBUG
void hashStreamSelfTest();
