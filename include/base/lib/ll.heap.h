/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#pragma once

// LavaLite min-heap:
// -------------------
// - Intrusive, like ll_list: the caller embeds a struct ll_heap_entry
//   inside its own struct (e.g. job_data) instead of the heap owning
//   separate storage for each element.
// - Binary heap stored as a flat array. Root (index 0) is always the
//   minimum element according to 'cmp'. No other ordering guarantee
//   exists between siblings/cousins.
// - 'owner' is the caller's struct pointer (e.g. a job_data*), passed
//   to 'cmp' so the heap itself never needs to know the caller's type
//   or which field it is ordered by.
// - idx is kept in sync on every swap so an element can be removed
//   from the middle of the heap (not just the root) in O(log n),
//   e.g. bkill on a job still waiting on its begin_time.

struct ll_heap_entry {
    int idx;     // position in the heap array, -1 if not in the heap
    void *owner; // caller's struct pointer, e.g. a job_data*
};

struct ll_heap {
    struct ll_heap_entry **data;
    int size;
    int cap;
    int (*cmp)(const void *a, const void *b); // a/b are 'owner' pointers
};

// Initialize an existing heap on the stack or inside another struct.
void ll_heap_init(struct ll_heap *h, int (*cmp)(const void *, const void *));

// Allocate and initialize a new heap on the heap.
// Caller must free with ll_heap_free().
struct ll_heap *ll_heap_create(int (*cmp)(const void *, const void *));

// Insert 'owner' into the heap. 'e' is the ll_heap_entry embedded in
// 'owner's struct; the caller allocates it, the heap only links it in.
void ll_heap_push(struct ll_heap *h, struct ll_heap_entry *e, void *owner);

// Return the minimum element's owner pointer without removing it.
// Returns NULL if the heap is empty.
void *ll_heap_peek(const struct ll_heap *h);

// Remove and return the minimum element's owner pointer.
// Returns NULL if the heap is empty.
void *ll_heap_pop(struct ll_heap *h);

// Remove a specific element from anywhere in the heap (not just the
// root). No-op if 'e' is not currently in the heap (idx == -1).
void ll_heap_remove(struct ll_heap *h, struct ll_heap_entry *e);

int ll_heap_is_empty(const struct ll_heap *h);
int ll_heap_count(const struct ll_heap *h);

// Free a heap-allocated ll_heap created with ll_heap_create().
// Optionally applies cleanup(owner) to each remaining element first.
// Does NOT free the owners themselves unless cleanup does so.
void ll_heap_free(struct ll_heap *h, void (*cleanup)(void *));

// Release the array and reinitialize an embedded ll_heap (stack/struct).
void ll_heap_clear(struct ll_heap *h, void (*cleanup)(void *));
