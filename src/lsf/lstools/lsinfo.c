/* lsinfo.c - display resource, type, and model information
 *
 * Copyright (C) 2024-2025 LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */

#include <lsf.h>

static void usage(const char *cmd);
static void print_resources(const struct lsInfo *info);
static void print_types(const struct lsInfo *info);
static void print_models(const struct lsInfo *info);
static const char *value_type_to_str(enum valueType valtype);

int main(int argc, char **argv)
{
    struct lsInfo *info;
    int cc;

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        return -1;
    }

    while ((cc = getopt(argc, argv, "Vh")) != -1) {
        switch (cc) {
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
        default:
            usage(argv[0]);
            return -1;
        }
    }

    if (optind < argc) {
        usage(argv[0]);
        return -1;
    }

    info = ls_info();
    if (info == NULL) {
        ls_perror("ls_info");
        return -1;
    }

    print_resources(info);
    putchar('\n');
    print_types(info);
    putchar('\n');
    print_models(info);

    return 0;
}

static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s [-h] [-V]\n", cmd);
}

static void print_resources(const struct lsInfo *info)
{
    int i;

    printf("%-16s %-10s %s\n", "RESOURCE_NAME", "TYPE", "DESCRIPTION");

    for (i = 0; i < info->nRes; i++) {
        const struct resItem *res;

        res = &info->resTable[i];

        printf("%-16s %-10s %s\n", res->name, value_type_to_str(res->valueType),
               res->des);
    }
}

static void print_types(const struct lsInfo *info)
{
    int i;

    printf("TYPE_NAME\n");

    for (i = 0; i < info->nTypes; i++) {
        printf("%s\n", info->hostTypes[i]);
    }
}

static void print_models(const struct lsInfo *info)
{
    int i;

    /* 16 chars for model, 10 for factor, 16 for arch */
    printf("%-16.16s %10s   %-16.16s\n", "MODEL_NAME", "CPU_FACTOR",
           "ARCHITECTURE");

    for (i = 0; i < info->nModels; i++) {
        const char *model;
        const char *arch;
        float factor;

        model = info->hostModels[i];
        arch = info->hostArchs[i];
        factor = info->cpuFactor[i];

        printf("%-16.16s %10.2f   %-16.16s\n", model, factor, arch);
    }
}

static const char *value_type_to_str(enum valueType valtype)
{
    switch (valtype) {
    case LS_NUMERIC:
        return "Numeric";
    case LS_BOOLEAN:
        return "Boolean";
    case LS_EXTERNAL:
        return "External";
    case LS_STRING:
    default:
        return "String";
    }
}
