/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

enum ll_bufsiz {
    LL_BUFSIZ_32 = 32,
    LL_BUFSIZ_64 = 64,
    LL_BUFSIZ_256 = 256,
    LL_BUFSIZ_512 = 512,
    LL_BUFSIZ_1K = 1024,
    LL_BUFSIZ_4K = 4096,
};

#define LL_MAX_PACKET_SIZE (64 * 1024 * 1024)
