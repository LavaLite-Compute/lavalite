#pragma once
/*
 * ll_bufsize.h — central buffer and string size definitions
 * --------------------------------------------------------
 * These constants replace ad-hoc MAX*LEN macros and avoid
 * reliance on stdio.h's BUFSIZ (implementation-dependent).
 *
 * All values are powers of two and easy to reason about.
 */

enum {
    LL_BUFSIZ_16 = 16,
    LL_BUFSIZ_32 = 32,
    LL_BUFSIZ_64 = 64,
    LL_BUFSIZ_128 = 128,
    LL_BUFSIZ_256 = 256,
    LL_BUFSIZ_512 = 512,
    LL_BUFSIZ_1K = 1024,
    LL_BUFSIZ_2K = 2048,
    LL_BUFSIZ_4K = 4096,
    LL_BUFSIZ_8K = 8192,
    LL_BUFSIZ_16K = 16384,
    LL_BUFSIZ_32K = 32768,
    LL_BUFSIZ_64K = 65536,
    /* Maximum size of a single environment variable stored in job spec.
     * 2 MiB is practically unlimited for real systems (EDA/Lmod etc.) while
     * still preventing pathological allocations caused by corrupt jobfiles.
     * This matches ARG_MAX ~= 2MB on Rocky9/Ubuntu24.x.
     */
    LL_ENVVAR_MAX = (2u * 1024u * 1024u)
};

/* Utility macro for kibibytes
 */
#define LL_KiB(n) ((size_t) (n) * 1024)

/*
 * Recommended usage:
 *   char line[LL_BUFSIZ_1K];
 *   char msgbuf[LL_BUFSIZ_4K];
 *   void *block = malloc(LL_BUFSIZ_64K);
 */
