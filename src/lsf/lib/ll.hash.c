/* ------------------------------------------------------------------------
 * LavaLite — High-Performance Job Scheduling Infrastructure
 *
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * ------------------------------------------------------------------------
 */

#include "lsf/lib/ll.hash.h"

// Return 1 if n is prime, 0 otherwise.
// Used to ensure the hash-table bucket count is a prime number.
//
// Prime bucket counts can reduce clustering for some hash functions and help
// avoid pathological patterns when keys share common factors. With FNV-1a this
// is not strictly required, but it improves distribution slightly and is cheap
// to compute during resize operations.
static int ll_is_prime(size_t n)
{
    size_t i;

    if (n < 2)
        return 0;
    if (n == 2 || n == 3)
        return 1;
    if (n % 2 == 0 || n % 3 == 0)
        return 0;

    for (i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return 0;
    }

    return 1;
}

// Return the smallest prime number >= n.
static size_t ll_next_prime(size_t n)
{
    if (n < 2)
        n = 2;

    while (!ll_is_prime(n))
        n++;

    return n;
}

// Compute a 64-bit FNV-1a hash of a NUL-terminated string.
// This uses the 32-bit FNV-1a variant internally and widens the result to
// 64 bits. This is fast and sufficient for hash-table indexing; the caller
// typically reduces the result modulo the bucket count.
uint64_t ll_hash_str(const char *s)
{
    uint32_t h = 2166136261u;  // 32-bit FNV-1a offset basis
    uint8_t c;

    while ((c = (uint8_t)*s++) != 0) {
        h ^= c;
        h *= 16777619u;        // 32-bit FNV-1a prime
    }

    return (uint64_t)h;
}

static size_t ll_bucket_index(const char *key, size_t nbuckets)
{
    uint64_t h = ll_hash_str(key);

    return (size_t)(h % nbuckets);
}

static int ll_hash_resize(struct ll_hash *ht, size_t new_buckets)
{
    struct ll_hash_entry **new_array;
    size_t i;

    new_buckets = ll_next_prime(new_buckets);

    new_array = calloc(new_buckets, sizeof(*new_array));
    if (!new_array)
        return -1;

    for (i = 0; i < ht->nbuckets; i++) {
        struct ll_hash_entry *ent = ht->buckets[i];

        while (ent) {
            struct ll_hash_entry *next = ent->next;
            size_t idx = ll_bucket_index(ent->key, new_buckets);

            ent->next = new_array[idx];
            new_array[idx] = ent;

            ent = next;
        }
    }

    free(ht->buckets);

    ht->buckets  = new_array;
    ht->nbuckets = new_buckets;

    return 0;
}

// Default load factor threshold for resizing.
// A load factor above ~0.75 causes average chain length to grow and lookup
// time to increase. A threshold around 0.7–0.8 is a widely used compromise
// between memory overhead and constant-time performance. If the caller passes
// <= 0, 0.75 is used as the default.
static int ll_hash_maybe_grow(struct ll_hash *ht)
{
    double load;

    if (ht->nbuckets == 0)
        return 0;

    load = (double)ht->nentries / (double)ht->nbuckets;
    if (load <= ht->max_load_factor)
        return 0;

    if (ll_hash_resize(ht, ht->nbuckets * 2) < 0)
        return -1;

    return 0;
}

int ll_hash_init(struct ll_hash *ht, size_t initial_buckets)
{
    if (initial_buckets == 0)
        initial_buckets = 11;

    initial_buckets = ll_next_prime(initial_buckets);

    ht->buckets = calloc(initial_buckets, sizeof(*ht->buckets));
    if (!ht->buckets)
        return -1;

    ht->nbuckets        = initial_buckets;
    ht->nentries        = 0;
    ht->max_load_factor =  0.75;   // fixed internal policy

    return 0;
}

struct ll_hash *ll_hash_create(size_t initial_buckets)
{
    struct ll_hash *ht;

    ht = calloc(1, sizeof(*ht));
    if (!ht)
        return NULL;

    if (ll_hash_init(ht, initial_buckets) < 0) {
        free(ht);
        return NULL;
    }

    return ht;
}

enum ll_hash_status ll_hash_insert(struct ll_hash *ht,
                                   const char *key,
                                   void *value,
                                   int allow_update)
{
    size_t idx;
    struct ll_hash_entry *ent;

    if (!ht || !key || ht->nbuckets == 0)
        return LL_HASH_EXISTS;

    (void)ll_hash_maybe_grow(ht);

    idx = ll_bucket_index(key, ht->nbuckets);

    // Find existing key.
    for (ent = ht->buckets[idx]; ent; ent = ent->next) {
        if (strcmp(ent->key, key) == 0) {
            if (allow_update) {
                ent->value = value;
                return LL_HASH_UPDATED;
            }
            return LL_HASH_EXISTS;
        }
    }

    // New key.
    ent = malloc(sizeof(*ent));
    if (!ent)
        return LL_HASH_EXISTS;

    ent->key = strdup(key);
    if (!ent->key) {
        free(ent);
        return LL_HASH_EXISTS;
    }

    ent->value = value;
    ent->next  = ht->buckets[idx];
    ht->buckets[idx] = ent;
    ht->nentries++;

    return LL_HASH_INSERTED;
}

void *ll_hash_search(struct ll_hash *ht, const char *key)
{
    size_t idx;
    struct ll_hash_entry *ent;

    if (!ht || !key || ht->nbuckets == 0)
        return NULL;

    idx = ll_bucket_index(key, ht->nbuckets);

    for (ent = ht->buckets[idx]; ent; ent = ent->next) {
        if (strcmp(ent->key, key) == 0)
            return ent->value;
    }

    return NULL;
}

void *ll_hash_remove(struct ll_hash *ht, const char *key)
{
    size_t idx;
    struct ll_hash_entry *ent;
    struct ll_hash_entry *prev = NULL;
    void *value = NULL;

    if (!ht || !key || ht->nbuckets == 0)
        return NULL;

    idx = ll_bucket_index(key, ht->nbuckets);
    ent = ht->buckets[idx];

    while (ent) {
        if (strcmp(ent->key, key) == 0) {
            if (prev)
                prev->next = ent->next;
            else
                ht->buckets[idx] = ent->next;

            value = ent->value;
            free(ent->key);
            free(ent);
            ht->nentries--;
            break;
        }

        prev = ent;
        ent  = ent->next;
    }

    return value;
}

void ll_hash_for_each(struct ll_hash *ht,
                      void (*fn)(struct ll_hash_entry *ent, void *user),
                      void *user)
{
    size_t i;

    if (!ht || !fn)
        return;

    for (i = 0; i < ht->nbuckets; i++) {
        struct ll_hash_entry *e = ht->buckets[i];

        while (e) {
            struct ll_hash_entry *next = e->next;

            // Safe because if fn() removes e via ll_hash_remove(), we never
            // touch *e again. 'next' remains valid.
            fn(e, user);
            e = next;
        }
    }
}

// Remove all entries from the hash table but keep the table object and its
// bucket array. Frees each entry node and its key string. An optional cleanup
// callback can free or otherwise release user-provided value pointers.
void ll_hash_clear(struct ll_hash *ht,
                   void (*cleanup)(struct ll_hash_entry *ent, void *user),
                   void *user)
{
    size_t i;

    if (!ht || !ht->buckets)
        return;

    for (i = 0; i < ht->nbuckets; i++) {
        struct ll_hash_entry *ent = ht->buckets[i];

        while (ent) {
            struct ll_hash_entry *next = ent->next;

            if (cleanup)
                cleanup(ent, user);

            free(ent->key);
            free(ent);

            ent = next;
        }

        ht->buckets[i] = NULL;
    }

    ht->nentries = 0;
}

// Free a hash table created with ll_hash_create().
// Calls ll_hash_clear() to release all entries and run the optional cleanup
// callback, then frees the bucket array and the ll_hash object itself.
void ll_hash_free(struct ll_hash *ht,
                  void (*cleanup)(struct ll_hash_entry *ent, void *user),
                  void *user)
{
    if (!ht)
        return;

    ll_hash_clear(ht, cleanup, user);
    free(ht->buckets);
    free(ht);
}
