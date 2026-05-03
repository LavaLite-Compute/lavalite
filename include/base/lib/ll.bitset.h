/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#pragma once

#include <stdint.h>

#define LL_BITSET_WORDS(nbits)  (((nbits) + 63) / 64)

struct ll_bitset {
    uint64_t *words;
    int       nwords;
};

void ll_bitset_init(struct ll_bitset *, uint64_t *, int);
void ll_bitset_set(struct ll_bitset *, int);
void ll_bitset_clr(struct ll_bitset *, int);
int  ll_bitset_get(const struct ll_bitset *, int);
void ll_bitset_zero(struct ll_bitset *);
void ll_bitset_copy(struct ll_bitset *, const struct ll_bitset *);
int  ll_bitset_any(const struct ll_bitset *);
