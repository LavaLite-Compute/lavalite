/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#pragma once

#include <stdint.h>

#include "base/lib/ll.list.h"

enum dep_type {
    DEP_DONE,
    DEP_EXIT,
    DEP_ENDED,
    DEP_AND,
    DEP_OR,
    DEP_NOT,
};

struct job_dep {
    struct ll_list_entry ent;
    enum dep_type type;
    int64_t job_id; /* valid only for DEP_DONE/DEP_EXIT/DEP_ENDED */
};

enum dep_parse_status {
    DEP_PARSE_OK = 0,
    DEP_PARSE_SYNTAX = -1,
    DEP_PARSE_UNKNOWN_JOB = -2,
};

/* Return 1 if job_id is known to exist, 0 otherwise.
 * Pass NULL to skip existence checking (syntax-only parse).
 */
typedef int (*dep_resolve_fn)(int64_t job_id, void *ctx);

/* Return 1 if the condition currently holds, 0 otherwise. */
typedef int (*dep_check_fn)(enum dep_type type, int64_t job_id, void *ctx);

/* Parse str into deps (a list the caller has already ll_list_init'd).
 * On any error the list is left empty (dep_list_free'd internally).
 * Returns DEP_PARSE_OK, DEP_PARSE_SYNTAX, or DEP_PARSE_UNKNOWN_JOB.
 */
int dep_parse(const char *str, struct ll_list *deps, dep_resolve_fn resolve,
             void *ctx);

/* Free every job_dep on the list and reinitialize it. */
void dep_list_free(struct ll_list *deps);

/* Evaluate a parsed dependency list. Returns 1 if satisfied, 0 otherwise. */
int dep_list_eval(const struct ll_list *deps, dep_check_fn check, void *ctx);
