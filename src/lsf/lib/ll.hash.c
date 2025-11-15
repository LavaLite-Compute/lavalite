
#include "hash.h"

static struct hash_entry *ht_find_slot(struct hash_table *, const char *);
static size_t next_prime(size_t);
static int is_prime(size_t);
static unsigned int hash(const char *, size_t);

struct hash_table *ht_create(size_t size)
{
    struct hash_table *ht = malloc(sizeof(struct hash_table));

    if (size == 0)
        ht->size = 11;

    ht->size = next_prime(size);
    ht->count = 0;
    ht->buckets = calloc(ht->size, sizeof(struct hash_entry *));
    return ht;
}

void ht_resize(struct hash_table *ht)
{
    size_t new_size = next_prime(ht->size * 2);
    struct hash_entry **new_buckets =
        calloc(new_size, sizeof(struct hash_entry *));

    for (size_t i = 0; i < ht->size; i++) {
        struct hash_entry *e = ht->buckets[i];
        while (e) {
            struct hash_entry *next = e->next;
            unsigned int idx = hash(e->key, new_size);
            e->next = new_buckets[idx];
            new_buckets[idx] = e;
            e = next;
        }
    }
    free(ht->buckets);
    ht->buckets = new_buckets;
    ht->size = new_size;
}

enum ht_status ht_insert(struct hash_table *ht, const char *key, void *value)
{
    if ((double) ht->count / ht->size > LOAD_FACTOR)
        ht_resize(ht);

    struct hash_entry *e = ht_find_slot(ht, key);
    if (e) {
        if (e->value == value)
            return HT_ALREADY_EXISTS;
        e->value = value;
        return HT_UPDATED;
    }

    unsigned int idx = hash(key, ht->size);
    struct hash_entry *new = malloc(sizeof(struct hash_entry));
    new->key = strdup(key);
    new->value = value;
    new->next = ht->buckets[idx];
    ht->buckets[idx] = new;
    ht->count++;
    return HT_INSERTED;
}

void *ht_search(struct hash_table *ht, const char *key)
{
    unsigned int idx = hash(key, ht->size);

    struct hash_entry *e = ht->buckets[idx];
    while (e) {
        if (strcmp(e->key, key) == 0)
            return e->value;
        e = e->next;
    }
    return NULL;
}

void *ht_remove(struct hash_table *ht, const char *key)
{
    unsigned int idx = hash(key, ht->size);
    struct hash_entry *prev = NULL;

    struct hash_entry *e = ht->buckets[idx];
    while (e) {
        if (strcmp(e->key, key) == 0) {
            void *val = e->value;
            if (prev)
                prev->next = e->next;
            else
                ht->buckets[idx] = e->next;
            free(e->key);
            free(e);
            ht->count--;
            return val;
        }
        prev = e;
        e = e->next;
    }
    return NULL;
}

// Traversal with safe deletion
void ht_for_each(struct hash_table *ht, void (*func)(struct hash_entry *))
{
    for (size_t i = 0; i < ht->size; i++) {
        struct hash_entry *e = ht->buckets[i];
        while (e) {
            struct hash_entry *next = e->next;
            /* Safe because:
             * - If func() called ht_remove(ht, e->key), e is freed, but we
             * never touch *e again.
             * - We do NOT allow removing anything except the current node; thus
             *   'next' is still valid (wasn’t freed).
             */
            func(e);
            e = next;
        }
    }
}

void ht_free(struct hash_table *ht, void (*func)(struct hash_entry *))
{
    for (size_t i = 0; i < ht->size; i++) {
        struct hash_entry *e = ht->buckets[i];
        while (e) {
            struct hash_entry *next = e->next;
            free(e->key);
            if (func)
                func(e);
            free(e);
            e = next;
        }
    }
    ht->count = 0;
    free(ht->buckets);
    free(ht);
}

// Static

static struct hash_entry *ht_find_slot(struct hash_table *ht, const char *key)
{
    unsigned int idx = hash(key, ht->size);
    struct hash_entry *e = ht->buckets[idx];
    while (e) {
        if (strcmp(e->key, key) == 0)
            return e;
        e = e->next;
    }
    return NULL;
}

static size_t next_prime(size_t n)
{
    while (!is_prime(n))
        ++n;
    return n;
}

static int is_prime(size_t n)
{
    if (n < 2)
        return 0;
    if (n == 2 || n == 3)
        return 1;
    if (n % 2 == 0 || n % 3 == 0)
        return 0;
    for (size_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return 0;
    }
    return 1;
}

static unsigned int hash(const char *key, size_t size)
{
    unsigned int h = 2166136261u;
    while (*key) {
        h ^= (unsigned char) *key++;
        h *= 16777619;
    }
    return h % size;
}
