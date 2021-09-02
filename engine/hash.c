#include "stdafx.h"
#include "system.h"
#include "hash.h"

//#include "stb/stretchy_buffer.h"
/*
static const int32_t kResizeFactor = 50;

void rsys_hmap_alloc(rsys_hash *hashMap)
{
	hashMap->hashes = malloc(sizeof(uint32_t) * hashMap->size * 2);
	memset(hashMap->hashes, 0, sizeof(uint32_t) * hashMap->size * 2);
	hashMap->values = (int32_t *)hashMap->hashes + hashMap->size;
	hashMap->resizeFactor = (hashMap->size * kResizeFactor) / 100;
}

void rsys_hmap_grow(rsys_hash *hashMap)
{
	uint32_t *oldBuffer = hashMap->hashes;
	uint32_t *oldHashes = oldBuffer;
	int32_t *oldValues = (int32_t *)oldBuffer + hashMap->size;
	int32_t oldCapacity = hashMap->size;
	hashMap->num = 0;
	hashMap->size *= 2;

	rsys_hmap_alloc(hashMap);

	for (int32_t i = 0; i < oldCapacity; ++i)
	{
		uint32_t hash = oldHashes[i];
		if (hash != 0)
		{
			int32_t value = oldValues[i];
			rsys_hm_insert(hashMap, hash, value);
		}
	}
	free(oldBuffer);
}

void rsys_hm_insert(rsys_hash *hashMap, uint32_t hash, int32_t index)
{
	if (hashMap)
	{
		if (hashMap->hashes == NULL)
		{
			hashMap->num = 0;
			hashMap->size = 16;
			rsys_hmap_alloc(hashMap);
		}

		if (hashMap->num >= hashMap->resizeFactor)
		{
			rsys_hmap_grow(hashMap);
		}
		hashMap->num++;
		uint32_t desiredPos = hash & (hashMap->size - 1);
		uint32_t pos = desiredPos;
		for (;;)
		{
			if (hashMap->hashes[pos] == 0)
			{
				hashMap->hashes[pos] = hash;
				hashMap->values[pos] = index;
				return;
			}

			pos = (pos + 1) & (hashMap->size - 1);
			if (pos == desiredPos)
			{
				rsys_print("HashMap full!\n");
				assert(0);
			}
		}
	}
}

int32_t rsys_hm_find(rsys_hash *hashMap, uint32_t hash)
{
	if (hashMap == NULL || hashMap->hashes == NULL)
		return -1;

	uint32_t desiredPos = hash & (hashMap->size - 1);
	uint32_t pos = desiredPos;

	for (;;)
	{
		if (hashMap->hashes[pos] == 0)
			return -1;

		if (hash == hashMap->hashes[pos])
			return hashMap->values[pos];

		pos = (pos + 1) & (hashMap->size - 1);
		if (pos == desiredPos)
			return -1;
	}
}
*/
