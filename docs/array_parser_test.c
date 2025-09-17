#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

// Include the parser code (normally you'd include a header file)
typedef struct {
    char name[256];
    int start;
    int end;
    int step;
    int job_limit;  // %N - concurrent job limit
} job_range_t;

int parse_job_range(const char *input, job_range_t *result);  // Forward declaration

// Test framework macros
static int test_count = 0;
static int test_passed = 0;

#define TEST_START(name) \
    do { \
        printf("Running test: %s ... ", name); \
        test_count++; \
    } while(0)

#define TEST_END() \
    do { \
        printf("PASSED\n"); \
        test_passed++; \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAILED\n  Expected: %d, Got: %d\n", (expected), (actual)); \
            return; \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("FAILED\n  Expected: '%s', Got: '%s'\n", (expected), (actual)); \
            return; \
        } \
    } while(0)

#define ASSERT_PARSE_SUCCESS(input) \
    do { \
        if (parse_job_range((input), &result) != 0) { \
            printf("FAILED\n  Parse failed for: '%s'\n", (input)); \
            return; \
        } \
    } while(0)

#define ASSERT_PARSE_FAIL(input) \
    do { \
        if (parse_job_range((input), &result) == 0) { \
            printf("FAILED\n  Parse should have failed for: '%s'\n", (input)); \
            return; \
        } \
    } while(0)

// Test functions
void test_basic_range() {
    TEST_START("basic_range");
    job_range_t result;

    ASSERT_PARSE_SUCCESS("job[1-10]");
    ASSERT_STR_EQ("job", result.name);
    ASSERT_EQ(1, result.start);
    ASSERT_EQ(10, result.end);
    ASSERT_EQ(1, result.step);
    ASSERT_EQ(-1, result.job_limit);

    TEST_END();
}

void test_with_j_flag() {
    TEST_START("with_j_flag");
    job_range_t result;

    ASSERT_PARSE_SUCCESS("-J\"worker[5-15]\"");
    ASSERT_STR_EQ("worker", result.name);
    ASSERT_EQ(5, result.start);
    ASSERT_EQ(15, result.end);
    ASSERT_EQ(1, result.step);
    ASSERT_EQ(-1, result.job_limit);

    TEST_END();
}

void test_with_step() {
    TEST_START("with_step");
    job_range_t result;

    ASSERT_PARSE_SUCCESS("task[1-100, 3]");
    ASSERT_STR_EQ("task", result.name);
    ASSERT_EQ(1, result.start);
    ASSERT_EQ(100, result.end);
    ASSERT_EQ(3, result.step);
    ASSERT_EQ(-1, result.job_limit);

    TEST_END();
}

void test_with_job_limit() {
    TEST_START("with_job_limit");
    job_range_t result;

    ASSERT_PARSE_SUCCESS("batch[10-50]%8");
    ASSERT_STR_EQ("batch", result.name);
    ASSERT_EQ(10, result.start);
    ASSERT_EQ(50, result.end);
    ASSERT_EQ(1, result.step);
    ASSERT_EQ(8, result.job_limit);

    TEST_END();
}

void test_with_step_and_limit() {
    TEST_START("with_step_and_limit");
    job_range_t result;

    ASSERT_PARSE_SUCCESS("-J \"compute[1-200, 5]%12\"");
    ASSERT_STR_EQ("compute", result.name);
    ASSERT_EQ(1, result.start);
    ASSERT_EQ(200, result.end);
    ASSERT_EQ(5, result.step);
    ASSERT_EQ(12, result.job_limit);

    TEST_END();
}

void test_whitespace_handling() {
    TEST_START("whitespace_handling");
    job_range_t result;

    ASSERT_PARSE_SUCCESS(" -J \"  test[ 1 - 10 , 2 ]% 4 \" ");
    ASSERT_STR_EQ("test", result.name);
    ASSERT_EQ(1, result.start);
    ASSERT_EQ(10, result.end);
    ASSERT_EQ(2, result.step);
    ASSERT_EQ(4, result.job_limit);

    TEST_END();
}

void test_single_job() {
    TEST_START("single_job");
    job_range_t result;

    ASSERT_PARSE_SUCCESS("single[5-5]");
    ASSERT_STR_EQ("single", result.name);
    ASSERT_EQ(5, result.start);
    ASSERT_EQ(5, result.end);
    ASSERT_EQ(1, result.step);
    ASSERT_EQ(-1, result.job_limit);

    TEST_END();
}

void test_invalid_syntax() {
    TEST_START("invalid_syntax");
    job_range_t result;

    // Missing bracket
    ASSERT_PARSE_FAIL("job1-10]");
    // Missing closing bracket
    ASSERT_PARSE_FAIL("job[1-10");
    // Invalid range
    ASSERT_PARSE_FAIL("job[10-5]");
    // Missing dash
    ASSERT_PARSE_FAIL("job[1,10]");
    // Invalid step
    ASSERT_PARSE_FAIL("job[1-10, 0]");
    // Invalid job limit
    ASSERT_PARSE_FAIL("job[1-10]%0");
    // Negative numbers
    ASSERT_PARSE_FAIL("job[-1-10]");

    TEST_END();
}

void test_edge_cases() {
    TEST_START("edge_cases");
    job_range_t result;

    // Large numbers
    ASSERT_PARSE_SUCCESS("big[1-999999]%100");
    ASSERT_EQ(999999, result.end);
    ASSERT_EQ(100, result.job_limit);

    // Large step
    ASSERT_PARSE_SUCCESS("skip[1-100, 50]");
    ASSERT_EQ(50, result.step);

    TEST_END();
}

void test_name_variations() {
    TEST_START("name_variations");
    job_range_t result;

    // Single character name
    ASSERT_PARSE_SUCCESS("a[1-5]");
    ASSERT_STR_EQ("a", result.name);

    // Name with numbers
    ASSERT_PARSE_SUCCESS("job123[1-10]");
    ASSERT_STR_EQ("job123", result.name);

    // Long name
    ASSERT_PARSE_SUCCESS("very_long_job_name_here[1-3]");
    ASSERT_STR_EQ("very_long_job_name_here", result.name);

    TEST_END();
}

// Parser implementation (normally in separate file)
int parse_job_range(const char *input, job_range_t *result) {
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
    // Trim trailing whitespace from name
    while (name_len > 0 && isspace(ptr[name_len - 1])) {
        name_len--;
    }

    if (name_len >= sizeof(result->name) || name_len == 0) {
        return -1; // Name too long or empty
    }
    strncpy(result->name, ptr, name_len);
    result->name[name_len] = '\0';

    // Move past the bracket
    ptr = bracket + 1;

    // Skip whitespace
    while (*ptr && isspace(*ptr)) {
        ptr++;
    }

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

        // Skip whitespace
        while (*ptr && isspace(*ptr)) {
            ptr++;
        }
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

int main() {
    printf("Running Job Parser Unit Tests\n");
    printf("=============================\n");

    test_basic_range();
    test_with_j_flag();
    test_with_step();
    test_with_job_limit();
    test_with_step_and_limit();
    test_whitespace_handling();
    test_single_job();
    test_invalid_syntax();
    test_edge_cases();
    test_name_variations();

    printf("\n=============================\n");
    printf("Tests passed: %d/%d\n", test_passed, test_count);

    if (test_passed == test_count) {
        printf("All tests PASSED! ✓\n");
        return 0;
    } else {
        printf("Some tests FAILED! ✗\n");
        return 1;
    }
}
