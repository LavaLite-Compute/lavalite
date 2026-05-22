/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <assert.h>
#include "base/lib/ll.bitset.h"

void ll_bitset_init(struct ll_bitset *bs, uint64_t *words, int nwords)
{
    assert(nwords > 0);
    bs->words = words;
    bs->nwords = nwords;
    memset(words, 0, (size_t) nwords * sizeof(uint64_t));
}

void ll_bitset_set(struct ll_bitset *bs, int i)
{
    int word = i / 64;
    int bit = i % 64;
    assert(i >= 0 && word < bs->nwords);
    bs->words[word] |= (1ULL << bit);
}

void ll_bitset_clr(struct ll_bitset *bs, int i)
{
    bs->words[i >> 6] &= ~(1ULL << (i & 63));
}

int ll_bitset_get(const struct ll_bitset *bs, int i)
{
    return (int) ((bs->words[i >> 6] >> (i & 63)) & 1);
}

void ll_bitset_zero(struct ll_bitset *bs)
{
    memset(bs->words, 0, (size_t) bs->nwords * sizeof(uint64_t));
}

void ll_bitset_copy(struct ll_bitset *dst, const struct ll_bitset *src)
{
    memcpy(dst->words, src->words, (size_t) src->nwords * sizeof(uint64_t));
}

int ll_bitset_any(const struct ll_bitset *bs)
{
    int i;
    for (i = 0; i < bs->nwords; i++)
        if (bs->words[i])
            return 1;
    return 0;
}
