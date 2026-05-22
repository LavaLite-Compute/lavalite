/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include <unistd.h>
#include <pwd.h>
#include <stdint.h>
#include <time.h>

int get_uid(const char *, uid_t *);
int millisleep(uint32_t);
size_t ll_strlcpy(char *, const char *, size_t);
size_t ll_strlcat(char *, const char *, size_t);
int ll_atoi(const char *, int *);
int ll_atoll(const char *, int64_t *);
const char *ctime2(time_t *);
int rd_poll(int, int);
struct passwd *getpwnam2(const char *);
struct passwd *getpwuid2(uid_t);
int install_signal_handler(int, void (*handler)(int), int);
int ll_set_limits(void);
int ll_str_to_sig(const char *);
const char *ll_sig_to_str(int);
