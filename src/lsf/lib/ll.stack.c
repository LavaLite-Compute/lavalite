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

#include "lsf/lib/ll.stack.h"

struct ll_stack *ll_stack_create(size_t initial_capacity)
{
    struct ll_stack *stack = calloc(1, sizeof(*stack));
    size_t cap;

    if (!stack)
        return NULL;

    cap = initial_capacity ? initial_capacity : 16;

    stack->items = calloc(cap, sizeof(void *));
    if (!stack->items) {
        free(stack);
        return NULL;
    }

    stack->capacity = cap;
    stack->size = 0;
    return stack;
}

void ll_stack_free(struct ll_stack *stack)
{
    if (!stack)
        return;

    free(stack->items);
    free(stack);
}

int ll_stack_push(struct ll_stack *stack, void *item)
{
    void **tmp;

    if (!stack)
        return -1;

    if (stack->size == stack->capacity) {
        size_t new_cap = stack->capacity ? stack->capacity * 2 : 16;

        tmp = realloc(stack->items, new_cap * sizeof(void *));
        if (!tmp)
            return -1;

        stack->items = tmp;
        stack->capacity = new_cap;
    }

    stack->items[stack->size++] = item;
    return 0;
}

void *ll_stack_pop(struct ll_stack *stack)
{
    if (!stack || stack->size == 0)
        return NULL;

    return stack->items[--stack->size];
}

int ll_stack_is_empty(const struct ll_stack *stack)
{
    if (!stack)
        return 1;

    return stack->size == 0;
}
