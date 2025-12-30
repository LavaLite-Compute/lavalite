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
 * ------------------------------------------------------------------------ */

#pragma once

#include "lsf/lib/ll.sys.h"

// LavaLite hash table:
// ---------------------
// - String-keyed, separate chaining.
// - Keys are duplicated with strdup() on insert and freed by the hash table.
// - Values are stored as void* and are NOT freed by default.
// - Bucket count is always a prime number.
// - Load factor threshold (0.75) is fixed internally and not part of the API.
//
// This table is intended as a fast lookup "index" for existing objects
// (jobs, hosts, queues, etc.) without owning them.

// Entry stored in a bucket chain.
// 'key' is owned by the hash table; 'value' is not.

struct ll_hash_entry {
    struct ll_hash_entry *next;  // next entry in chain
    char *key;                   // strdup()'d key, freed by table
    void *value;                 // user-supplied pointer, not freed by table
};

// Hash table structure.
struct ll_hash {
    struct ll_hash_entry **buckets;   // bucket array
    size_t nbuckets;                  // number of buckets (always prime)
    size_t nentries;                  // number of entries
    double max_load_factor;           // fixed internally to ~0.75
};

// Compute a 64-bit FNV-1a hash of a NUL-terminated string.
// Fast, deterministic, non-cryptographic. Caller reduces modulo nbuckets.

uint64_t ll_hash_str(const char *s);

// Initialize an existing hash table (e.g., stack-allocated or embedded).
// 'initial_buckets' is rounded up to the next prime. If zero, defaults to 11.
// Returns 0 on success, -1 on allocation failure.

int ll_hash_init(struct ll_hash *ht, size_t initial_buckets);

// Allocate and initialize a new heap-allocated hash table.
// Returns NULL on failure. Caller must destroy with ll_hash_free().

struct ll_hash *ll_hash_create(size_t initial_buckets);

// Insert or update a (key, value) pair.
// - Key string is copied with strdup().
// - If allow_update != 0 and the key exists, the value is replaced.
// Returns:
//   LL_HASH_INSERTED (new key)
//   LL_HASH_UPDATED  (existing key, value replaced)
//   LL_HASH_EXISTS   (existing key, value unchanged OR allocation failure)

enum ll_hash_status {
    LL_HASH_INSERTED = 0,
    LL_HASH_UPDATED,
    LL_HASH_EXISTS
};

enum ll_hash_status ll_hash_insert(struct ll_hash *,
                                   const char *,
                                   void *,
                                   int allow_update);

// Lookup value by key. Returns NULL if not found.
void *ll_hash_search(struct ll_hash *, const char *);

// Remove entry by key.
// Frees the key string and entry node. Does NOT free the value.
// Returns the stored value pointer, or NULL if the key is not present.
void *ll_hash_remove(struct ll_hash *, const char *);

// Call fn(entry, user) for every entry in the table.
// Safe for deletion of the CURRENT entry inside the callback.
// Not safe for arbitrary modification of other entries.
void ll_hash_for_each(struct ll_hash *ht,
                      void (*fn)(struct ll_hash_entry *, void *user),
                      void *user);

// Remove all entries but keep the hash table object and bucket array.
// Frees each entry and its key. Optional cleanup callback can free user values.
// After this call the table is empty and reusable.
void ll_hash_clear(struct ll_hash *ht,
                   void (*cleanup)(struct ll_hash_entry *, void *user),
                   void *user);

// Free a heap-allocated hash table created with ll_hash_create().
// Internally calls ll_hash_clear() to release entries, then frees the bucket
// array and the ll_hash object itself.
void ll_hash_free(struct ll_hash *ht,
                  void (*cleanup)(struct ll_hash_entry *, void *),
                  void *);
