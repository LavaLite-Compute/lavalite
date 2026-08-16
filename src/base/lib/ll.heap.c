/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>

#include "base/lib/ll.heap.h"

static void ll_heap_swap(struct ll_heap *h, int i, int j)
{
    struct ll_heap_entry *tmp;

    tmp = h->data[i];
    h->data[i] = h->data[j];
    h->data[j] = tmp;
    h->data[i]->idx = i;
    h->data[j]->idx = j;
}

static void ll_heap_sift_up(struct ll_heap *h, int i)
{
    int parent;

    while (i > 0) {
        parent = (i - 1) / 2;
        if (h->cmp(h->data[parent]->owner, h->data[i]->owner) <= 0)
            break;

        ll_heap_swap(h, i, parent);
        i = parent;
    }
}

static void ll_heap_sift_down(struct ll_heap *h, int i)
{
    int left, right, smallest;

    for (;;) {
        left = 2 * i + 1;
        right = 2 * i + 2;
        smallest = i;

        if (left < h->size &&
            h->cmp(h->data[left]->owner, h->data[smallest]->owner) < 0)
            smallest = left;
        if (right < h->size &&
            h->cmp(h->data[right]->owner, h->data[smallest]->owner) < 0)
            smallest = right;
        if (smallest == i)
            break;

        ll_heap_swap(h, i, smallest);
        i = smallest;
    }
}

void ll_heap_init(struct ll_heap *h, int (*cmp)(const void *, const void *))
{
    h->data = NULL;
    h->size = 0;
    h->cap = 0;
    h->cmp = cmp;
}

struct ll_heap *ll_heap_create(int (*cmp)(const void *, const void *))
{
    struct ll_heap *h;

    h = calloc(1, sizeof(*h));
    if (!h)
        return NULL;

    ll_heap_init(h, cmp);

    return h;
}

void ll_heap_push(struct ll_heap *h, struct ll_heap_entry *e, void *owner)
{
    if (h->size == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 64;
        h->data = realloc(h->data, h->cap * sizeof(*h->data));
    }

    e->owner = owner;
    e->idx = h->size;
    h->data[h->size] = e;
    h->size++;

    ll_heap_sift_up(h, e->idx);
}

void *ll_heap_peek(const struct ll_heap *h)
{
    if (h->size == 0)
        return NULL;

    return h->data[0]->owner;
}

void *ll_heap_pop(struct ll_heap *h)
{
    struct ll_heap_entry *e;
    void *owner;

    if (h->size == 0)
        return NULL;

    e = h->data[0];
    owner = e->owner;
    e->idx = -1;

    h->size--;
    if (h->size > 0) {
        h->data[0] = h->data[h->size];
        h->data[0]->idx = 0;
        ll_heap_sift_down(h, 0);
    }

    return owner;
}

void ll_heap_remove(struct ll_heap *h, struct ll_heap_entry *e)
{
    int i;

    i = e->idx;
    if (i < 0)
        return;

    e->idx = -1;
    h->size--;

    if (i == h->size)
        return;

    h->data[i] = h->data[h->size];
    h->data[i]->idx = i;

    // Only one of these will actually move the element; harmless to
    // call both since the other becomes a no-op on the first check.
    ll_heap_sift_down(h, i);
    ll_heap_sift_up(h, i);
}

int ll_heap_is_empty(const struct ll_heap *h)
{
    return h->size == 0;
}

int ll_heap_count(const struct ll_heap *h)
{
    return h->size;
}

void ll_heap_clear(struct ll_heap *h, void (*cleanup)(void *))
{
    int i;

    for (i = 0; i < h->size; i++) {
        h->data[i]->idx = -1;
        if (cleanup)
            cleanup(h->data[i]->owner);
    }

    free(h->data);
    h->data = NULL;
    h->size = 0;
    h->cap = 0;
}

void ll_heap_free(struct ll_heap *h, void (*cleanup)(void *))
{
    if (!h)
        return;

    ll_heap_clear(h, cleanup);
    free(h);
}
