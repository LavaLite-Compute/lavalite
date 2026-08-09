/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "batch/lib/dependency.h"

#define DEP_MAX_STACK 16

struct dep_parser {
    const char *p;
    struct ll_list *deps;
    dep_resolve_fn resolve;
    void *ctx;
    int error;
};

static void parse_expr(struct dep_parser *dp);
static void parse_term(struct dep_parser *dp);
static void parse_factor(struct dep_parser *dp);
static void parse_funccall(struct dep_parser *dp);

static void skip_ws(struct dep_parser *dp)
{
    while (isspace((unsigned char) *dp->p)) {
        dp->p++;
    }
}

static void emit(struct dep_parser *dp, enum dep_type type, int64_t job_id)
{
    struct job_dep *d;

    if (dp->error) {
        return;
    }

    d = malloc(sizeof(*d));
    if (d == NULL) {
        dp->error = DEP_PARSE_SYNTAX;
        return;
    }

    d->type = type;
    d->job_id = job_id;
    ll_list_append(dp->deps, &d->ent);
}

/* expr := term ('||' term)* */
static void parse_expr(struct dep_parser *dp)
{
    parse_term(dp);
    skip_ws(dp);

    while (!dp->error && dp->p[0] == '|' && dp->p[1] == '|') {
        dp->p += 2;
        parse_term(dp);
        emit(dp, DEP_OR, 0);
        skip_ws(dp);
    }
}

/* term := factor ('&&' factor)* */
static void parse_term(struct dep_parser *dp)
{
    parse_factor(dp);
    skip_ws(dp);

    while (!dp->error && dp->p[0] == '&' && dp->p[1] == '&') {
        dp->p += 2;
        parse_factor(dp);
        emit(dp, DEP_AND, 0);
        skip_ws(dp);
    }
}

/* factor := '!' factor | '(' expr ')' | funccall */
static void parse_factor(struct dep_parser *dp)
{
    skip_ws(dp);
    if (dp->error) {
        return;
    }

    if (*dp->p == '!') {
        dp->p++;
        parse_factor(dp);
        emit(dp, DEP_NOT, 0);
        return;
    }

    if (*dp->p == '(') {
        dp->p++;
        parse_expr(dp);
        skip_ws(dp);
        if (dp->error) {
            return;
        }
        if (*dp->p != ')') {
            dp->error = DEP_PARSE_SYNTAX;
            return;
        }
        dp->p++;
        return;
    }

    parse_funccall(dp);
}

/* funccall := ('done'|'exit'|'ended') '(' NUMBER ')' */
static void parse_funccall(struct dep_parser *dp)
{
    enum dep_type type;
    int64_t job_id;
    char *end;

    if (strncmp(dp->p, "done", 4) == 0) {
        type = DEP_DONE;
        dp->p += 4;
    } else if (strncmp(dp->p, "exit", 4) == 0) {
        type = DEP_EXIT;
        dp->p += 4;
    } else if (strncmp(dp->p, "ended", 5) == 0) {
        type = DEP_ENDED;
        dp->p += 5;
    } else {
        dp->error = DEP_PARSE_SYNTAX;
        return;
    }

    skip_ws(dp);
    if (*dp->p != '(') {
        dp->error = DEP_PARSE_SYNTAX;
        return;
    }
    dp->p++;
    skip_ws(dp);

    job_id = strtoll(dp->p, &end, 10);
    if (end == dp->p) {
        dp->error = DEP_PARSE_SYNTAX;
        return;
    }
    dp->p = end;

    skip_ws(dp);
    if (*dp->p != ')') {
        dp->error = DEP_PARSE_SYNTAX;
        return;
    }
    dp->p++;

    if (dp->resolve != NULL && dp->resolve(job_id, dp->ctx) == 0) {
        dp->error = DEP_PARSE_UNKNOWN_JOB;
        return;
    }

    emit(dp, type, job_id);
}

int dep_parse(const char *str, struct ll_list *deps, dep_resolve_fn resolve,
              void *ctx)
{
    struct dep_parser dp;

    dp.p = str;
    dp.deps = deps;
    dp.resolve = resolve;
    dp.ctx = ctx;
    dp.error = DEP_PARSE_OK;

    parse_expr(&dp);
    skip_ws(&dp);

    if (!dp.error && *dp.p != '\0') {
        dp.error = DEP_PARSE_SYNTAX;
    }

    if (dp.error) {
        dep_list_free(deps);
    }

    return dp.error;
}

void dep_list_free(struct ll_list *deps)
{
    ll_list_clear(deps, free);
}

int dep_list_eval(const struct ll_list *deps, dep_check_fn check, void *ctx)
{
    int stack[DEP_MAX_STACK];
    int sp;
    struct ll_list_entry *e;
    struct job_dep *d;
    int a, b;

    sp = 0;

    for (e = deps->head; e; e = e->next) {
        d = (struct job_dep *) e;

        switch (d->type) {
        case DEP_AND:
            b = stack[--sp];
            a = stack[--sp];
            stack[sp++] = a && b;
            break;
        case DEP_OR:
            b = stack[--sp];
            a = stack[--sp];
            stack[sp++] = a || b;
            break;
        case DEP_NOT:
            a = stack[--sp];
            stack[sp++] = !a;
            break;
        default:
            stack[sp++] = check(d->type, d->job_id, ctx);
            break;
        }
    }

    return stack[--sp];
}
