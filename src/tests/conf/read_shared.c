#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 256
#define MAX_FIELDS 8

typedef enum { NONE, CLUSTER, HOSTTYPE, HOSTMODEL, RESOURCE } Section;

static inline void trim(char *s)
{
    char *p = s;
    while (isspace(*p))
        p++;
    memmove(s, p, strlen(p) + 1);
    p = s + strlen(s) - 1;
    while (p > s && isspace(*p))
        *p-- = '\0';
}

static inline int find_field(const char *field, char *fields[], int count)
{
    for (int i = 0; i < count; i++)
        if (strcmp(fields[i], field) == 0)
            return i;
    return -1;
}

static inline void parse_lsf_shared(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("fopen");
        return;
    }

    char line[MAX_LINE];
    Section current = NONE;
    char *fields[MAX_FIELDS];
    int field_count = 0;

    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;

        if (strncmp(line, "Begin ", 6) == 0) {
            if (strstr(line, "Cluster"))
                current = CLUSTER;
            else if (strstr(line, "HostType"))
                current = HOSTTYPE;
            else if (strstr(line, "HostModel"))
                current = HOSTMODEL;
            else if (strstr(line, "Resource"))
                current = RESOURCE;
            field_count = 0;
            continue;
        }

        if (strncmp(line, "End ", 4) == 0) {
            current = NONE;
            continue;
        }

        // Header line with keywords
        if (strchr(line, '#')) {
            char *token = strtok(line, " \t");
            while (token && field_count < MAX_FIELDS) {
                fields[field_count++] = strdup(token);
                token = strtok(NULL, " \t");
            }
            continue;
        }

        // Data line
        char *tokens[MAX_FIELDS];
        int tok_count = 0;
        char *token = strtok(line, " \t");
        while (token && tok_count < MAX_FIELDS) {
            tokens[tok_count++] = token;
            token = strtok(NULL, " \t");
        }

        if (current == HOSTMODEL) {
            int i_model = find_field("MODELNAME", fields, field_count);
            int i_factor = find_field("CPUFACTOR", fields, field_count);
            int i_arch = find_field("ARCHITECTURE", fields, field_count);
            printf("HostModel: %s, CPUFactor: %s, Arch: %s\n",
                   i_model >= 0 ? tokens[i_model] : "?",
                   i_factor >= 0 ? tokens[i_factor] : "?",
                   i_arch >= 0 ? tokens[i_arch] : "?");
        }

        // Add similar logic for RESOURCE or other sections if needed
    }

    fclose(f);
}

int main(void)
{
    parse_lsf_shared("lsf.shared");
    return 0;
}
