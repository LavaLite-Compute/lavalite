/*
 * hog - consume memory and cpu for cgroup testing
 *
 * Usage: hog [mem_mb [seconds]]
 *
 *   mem_mb   - megabytes to allocate and touch (default 100)
 *   seconds  - how long to burn cpu (default 10)
 *
 * All allocated memory is touched (memset) so it is fully resident.
 * CPU is burned in a tight loop for the given duration.
 *
 * Example:
 *   hog 512 30    allocate 512MB, burn cpu for 30 seconds
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[])
{
    int mem_mb  = 100;
    int seconds = 10;

    if (argc > 1)
        mem_mb = atoi(argv[1]);
    if (argc > 2)
        seconds = atoi(argv[2]);

    if (mem_mb <= 0 || seconds <= 0) {
        fprintf(stderr, "usage: hog [mem_mb [seconds]]\n");
        return 1;
    }

    size_t sz = (size_t) mem_mb * 1024 * 1024;
    char *p = malloc(sz);
    if (!p) {
        perror("malloc");
        return 1;
    }

    /* touch every page so memory is fully resident */
    memset(p, 0xAA, sz);

    /* burn cpu */

    struct timespec t0, now;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if ((now.tv_sec - t0.tv_sec) >= seconds)
            break;
    }

    free(p);
    return 0;
}
