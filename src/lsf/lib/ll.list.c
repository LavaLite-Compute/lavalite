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

#include <stdlib.h>
#include "ll_list.h"

void ll_list_init(struct ll_list *lst)
{
    lst->head = NULL;
    lst->tail = NULL;
    lst->count = 0;
}

struct ll_list *ll_list_create(void)
{
    struct ll_list *lst = calloc(1, sizeof(*lst));
    if (!lst)
        return NULL;

    ll_list_init(lst);
    return lst;
}

void ll_list_append(struct ll_list *lst, struct ll_list_entry *ent)
{
    ent->next = NULL;
    ent->prev = lst->tail;

    if (lst->tail)
        lst->tail->next = ent;
    else
        lst->head = ent;

    lst->tail = ent;
    lst->count++;
}

void ll_list_remove(struct ll_list *lst, struct ll_list_entry *ent)
{
    if (!ent)
        return;

    if (ent->prev)
        ent->prev->next = ent->next;
    else
        lst->head = ent->next;

    if (ent->next)
        ent->next->prev = ent->prev;
    else
        lst->tail = ent->prev;

    ent->next = NULL;
    ent->prev = NULL;
    lst->count--;
}

int ll_list_is_empty(const struct ll_list *lst)
{
    return lst->head == NULL;
}

int ll_list_count(const struct ll_list *lst)
{
    return lst->count;
}

void ll_list_push(struct ll_list *lst, struct ll_list_entry *ent)
{
    ent->prev = NULL;
    ent->next = lst->head;

    if (lst->head)
        lst->head->prev = ent;
    else
        lst->tail = ent;

    lst->head = ent;
    lst->count++;
}

struct ll_list_entry *ll_list_pop(struct ll_list *lst)
{
    struct ll_list_entry *ent = lst->head;

    if (!ent)
        return NULL;

    lst->head = ent->next;
    if (lst->head)
        lst->head->prev = NULL;
    else
        lst->tail = NULL;

    ent->next = NULL;
    ent->prev = NULL;
    lst->count--;

    return ent;
}

struct ll_list_entry *ll_list_dequeue(struct ll_list *lst)
{
    return ll_list_pop(lst);
}

void ll_list_clear(struct ll_list *lst,
                   void (*cleanup)(struct ll_list_entry *ent))
{
    struct ll_list_entry *e = lst->head;
    struct ll_list_entry *next;

    while (e) {
        next = e->next;

        if (cleanup)
            cleanup(e);

        e->next = NULL;
        e->prev = NULL;

        e = next;
    }

    ll_list_init(lst);
}

void ll_list_free(struct ll_list *lst,
                  void (*cleanup)(struct ll_list_entry *ent))
{
    if (!lst)
        return;

    ll_list_clear(lst, cleanup);
    free(lst);
}
