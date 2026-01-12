/* ------------------------------------------------------------------------
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

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * ------------------------------------------------------------------------
 */

#pragma once
#include "lsf/lib/ll.sys.h"

// Doubly-linked list for LavaLite.
//
// Conceptual layout on the x-axis:
//
//   head                              tail
//    |                                 |
//    v                                 v
//   [e0] <-> [e1] <-> [e2] <-> ... <-> [eN]
//
//   next: move right  (towards increasing x, towards tail)
//   prev: move left   (towards decreasing x, towards head)

struct ll_list_entry {
    struct ll_list_entry *next;  // move right  (towards tail)
    struct ll_list_entry *prev;  // move left   (towards head)
};

struct ll_list {
    struct ll_list_entry *head;  // leftmost element
    struct ll_list_entry *tail;  // rightmost element
    int count;
};

// Initialize an existing list on the stack or inside another struct.
void ll_list_init(struct ll_list *lst);

// Allocate and initialize a new list on the heap.
// Caller must free with ll_list_free().
struct ll_list *ll_list_create(void);

// Append entry at the tail (right).
void ll_list_append(struct ll_list *lst, struct ll_list_entry *e);

// Remove an entry from the list (does not free it).
void ll_list_remove(struct ll_list *lst, struct ll_list_entry *e);

// True if the list has no elements.
int ll_list_is_empty(const struct ll_list *lst);

// Number of elements in the list.
int ll_list_count(const struct ll_list *lst);

// Stack-style push/pop at HEAD (left side).
void ll_list_push(struct ll_list *lst, struct ll_list_entry *e);
struct ll_list_entry *ll_list_pop(struct ll_list *lst);

// Queue-style dequeue from HEAD (left).
// (Alias for popping from head; API kept for readability where it's a queue.)
struct ll_list_entry *ll_list_dequeue(struct ll_list *lst);

// Free the list object itself (if heap-allocated), optionally applying a
// cleanup callback to each entry (e.g., to free container structs).
void ll_list_free(struct ll_list *lst, void (*cleanup)(void *));

// Free the entries and reinitialize the list
void ll_list_clear(struct ll_list *, void (*cleanup)(void *));

// Simple forward iteration helper but just optional sugar.
// Caller-supplied callback runs from head → tail.
static inline void ll_list_foreach(struct ll_list *lst,
                                   void (*fn)(struct ll_list_entry *))
{
    struct ll_list_entry *e;

    for (e = lst->head; e; e = e->next)
        fn(e);
}

// Reverse iteration helper: tail → head.
static inline void ll_list_foreach_reverse(struct ll_list *lst,
                                           void (*fn)(struct ll_list_entry *))
{
    struct ll_list_entry *e;

    for (e = lst->tail; e; e = e->prev)
        fn(e);
}
