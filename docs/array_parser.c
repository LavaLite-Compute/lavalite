
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char name[256];
    int start;
    int end;
    int step;
    int job_limit;  // %N - concurrent job limit
} job_range_t;

int
parse_job_range(const char *input, job_range_t *result)
{
    // Initialize result
    memset(result, 0, sizeof(job_range_t));
    result->step = 1; // default step
    result->job_limit = -1; // default: no limit

    // Skip -J prefix if present
    const char *ptr = input;
    if (strncmp(ptr, "-J", 2) == 0) {
        ptr += 2;
    }

    // Skip whitespace and quotes
    while (*ptr && (isspace(*ptr) || *ptr == '"')) {
        ptr++;
    }

    // Find the opening bracket
    const char *bracket = strchr(ptr, '[');
    if (!bracket) {
        return -1; // No bracket found
    }

    // Extract name
    int name_len = bracket - ptr;
    if (name_len >= sizeof(result->name)) {
        return -1; // Name too long
    }
    strncpy(result->name, ptr, name_len);
    result->name[name_len] = '\0';

    // Move past the bracket
    ptr = bracket + 1;

    // Parse start number
    char *endptr;
    result->start = strtol(ptr, &endptr, 10);
    if (endptr == ptr) {
        return -1; // No valid number
    }
    ptr = endptr;

    // Skip whitespace
    while (*ptr && isspace(*ptr)) {
        ptr++;
    }

    // Expect dash
    if (*ptr != '-') {
        return -1; // No dash found
    }
    ptr++;

    // Skip whitespace
    while (*ptr && isspace(*ptr)) {
        ptr++;
    }

    // Parse end number
    result->end = strtol(ptr, &endptr, 10);
    if (endptr == ptr) {
        return -1; // No valid number
    }
    ptr = endptr;

    // Skip whitespace
    while (*ptr && isspace(*ptr)) {
        ptr++;
    }

    // Check for comma (optional step)
    if (*ptr == ',') {
        ptr++;
        // Skip whitespace
        while (*ptr && isspace(*ptr)) {
            ptr++;
        }

        // Parse step
        result->step = strtol(ptr, &endptr, 10);
        if (endptr == ptr || result->step <= 0) {
            return -1; // Invalid step
        }
        ptr = endptr;
    }

    // Skip whitespace
    while (*ptr && isspace(*ptr)) {
        ptr++;
    }

    // Expect closing bracket
    if (*ptr != ']') {
        return -1; // No closing bracket
    }
    ptr++;

    // Skip whitespace
    while (*ptr && isspace(*ptr)) {
        ptr++;
    }

    // Check for % (optional job limit)
    if (*ptr == '%') {
        ptr++;

        // Parse job limit
        result->job_limit = strtol(ptr, &endptr, 10);
        if (endptr == ptr || result->job_limit <= 0) {
            return -1; // Invalid job limit
        }
        ptr = endptr;
    }

    // Validate range
    if (result->start > result->end || result->start < 0 || result->end < 0) {
        return -1; // Invalid range
    }

    return 0; // Success
}

void
print_jobs(const job_range_t *range)
{
    printf("Job name: %s\n", range->name);
    printf("Range: %d-%d, step: %d", range->start, range->end, range->step);
    if (range->job_limit > 0) {
        printf(", concurrent limit: %d", range->job_limit);
    }
    printf("\n");

    printf("Jobs: ");
    int job_count = 0;
    for (int i = range->start; i <= range->end; i += range->step) {
        printf("%s%d", range->name, i);
        job_count++;
        if (i + range->step <= range->end) {
            printf(", ");
        }
    }
    printf("\n");

    if (range->job_limit > 0) {
        printf("Total jobs: %d, will run %d at a time\n", job_count, range->job_limit);
    } else {
        printf("Total jobs: %d, no concurrent limit\n", job_count);
    }
}

int
main(void)
{
    job_range_t result;
    const char *test_cases[] = {
        "-J \"name[1-100]\"",
        "-J\"name[1-100, 2]\"",
        "-J\"name[1-100, 2]%5\"",
        "job[5-10]%3",
        "test[1-5, 3]%2",
        "-J \"worker[10-20, 5]%4\"",
        "batch[1-50]%10",
        NULL
    };

    for (int i = 0; test_cases[i]; i++) {
        printf("Parsing: %s\n", test_cases[i]);

        if (parse_job_range(test_cases[i], &result) == 0) {
            print_jobs(&result);
        } else {
            printf("Error: Failed to parse\n");
        }
        printf("\n");
    }

    return 0;
}
