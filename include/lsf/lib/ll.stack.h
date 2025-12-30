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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * ------------------------------------------------------------------------ */

#pragma once

#include "lsf/lib/ll.sys.h"

struct ll_stack {
    void **items; // pointer to array of pointers storing the objects
    size_t size;       /* number of elements currently stored */
    size_t capacity;   /* allocated capacity of items[] */
};

/* Create a new stack. If initial_capacity is 0, a default is used.
 * Returns NULL on allocation failure.
 */
struct ll_stack *ll_stack_create(size_t);

/* Free the stack structure and its internal buffer.
 * Does NOT free the individual items.
 */
void ll_stack_free(struct ll_stack *);

/* Push an item onto the stack.
 * Returns 0 on success, -1 on allocation failure.
 */
int ll_stack_push(struct ll_stack *, void *);

/* Pop the top item from the stack.
 * Returns the item pointer, or NULL if the stack is empty.
 */
void *ll_stack_pop(struct ll_stack *);

/* True if the stack has no elements. */
int ll_stack_is_empty(const struct ll_stack *);
