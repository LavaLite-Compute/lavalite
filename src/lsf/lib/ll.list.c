
#include "list.h"

void list_init(struct list *lst)
{
    lst->head = NULL;
    lst->tail = NULL;
    lst->count = 0;
}

struct list *list_init2(void)
{
    struct list *lst = calloc(1, sizeof(struct list));
    list_init(lst);
    return lst;
}

void list_append(struct list *lst, struct list_entry *ent)
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

void list_append2(struct list *lst, struct list_entry *ent)
{
    if (lst->head == NULL) {
        assert(lst->tail == NULL);
        lst->head = lst->tail = ent;
        return;
    }
    ent->next = NULL;
    lst->tail->next = ent;
    lst->tail = ent;
    lst->count++;
}

void list_remove(struct list *lst, struct list_entry *ent)
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

    lst->count--;
}

int list_is_empty(struct list *lst)
{
    return lst->head == NULL;
}

int list_count(struct list *lst)
{
    return lst->count;
}

/* walk right
 *
 for (struct list_entry *e = lst->head; e; e = e->next) {

 }
*/

/* walk left
 *
 for (struct list_entry *e = lst->tail; e; e = e->prev) {
  }
 *
 */

void list_push(struct list *lst, struct list_entry *ent)
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

struct list_entry *list_pop(struct list *lst)
{
    struct list_entry *ent = lst->head;
    if (!ent)
        return 0;

    lst->head = ent->next;
    if (lst->head)
        lst->head->prev = NULL;
    else
        lst->tail = NULL;

    lst->count--;
    ent->next = NULL;
    ent->prev = NULL;

    return ent;
}

struct list_entry *list_dequeue(struct list *lst)
{
    if (!lst->tail)
        return NULL;

    struct list_entry *ent = lst->tail;

    lst->tail = ent->prev; // NULL
    if (lst->tail)
        lst->tail->next = NULL;
    else
        lst->head = NULL; // list is now empty

    lst->count--;

    ent->next = NULL;
    ent->prev = NULL;

    return ent;
}
void list_free(struct list *lst, void (*cleanup)(struct list_entry *))
{
    struct list_entry *current = lst->head;
    while (current != NULL) {
        struct list_entry *next = current->next;

        if (cleanup) {
            cleanup(current); // Custom cleanup logic
        }

        free(current); // Free the node itself
        current = next;
    }
    list_init(lst); // Reset the list
}
