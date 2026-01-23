/*
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#include "lsf/lib/lib.h"

// lib.sig.c

#include <signal.h>
#include <string.h>
#include <ctype.h>

struct ll_sigmap {
    int sig;
    const char *name;    // lowercase canonical name
};

static const struct ll_sigmap ll_sig_table[] = {
    { SIGHUP,  "hup"  },
    { SIGINT,  "int"  },
    { SIGQUIT, "quit" },
    { SIGILL,  "ill"  },
    { SIGABRT, "abrt" },
    { SIGFPE,  "fpe"  },
    { SIGKILL, "kill" },
    { SIGSEGV, "segv" },
    { SIGPIPE, "pipe" },
    { SIGALRM, "alrm" },
    { SIGTERM, "term" },
    { SIGUSR1, "usr1" },
    { SIGUSR2, "usr2" },
    { SIGCHLD, "chld" },
    { SIGCONT, "cont" },
    { SIGSTOP, "stop" },
    { SIGTSTP, "tstp" }
};

static int
ll_streq_nocase(const char *a, const char *b)
{
    unsigned char ca, cb;

    while (*a && *b) {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb))
            return 0;
    }

    return (*a == '\0' && *b == '\0');
}

const char *ll_sig_to_str(int sig)
{
    size_t i;

    for (i = 0; i < sizeof(ll_sig_table) / sizeof(ll_sig_table[0]); i++) {
        if (ll_sig_table[i].sig == sig)
            return ll_sig_table[i].name;
    }

    return "unknown";
}

int ll_str_to_sig(const char *s)
{
    size_t i;

    if (!s || !*s)
        return -1;

    for (i = 0; i < sizeof(ll_sig_table) / sizeof(ll_sig_table[0]); i++) {
        if (ll_streq_nocase(ll_sig_table[i].name, s))
            return ll_sig_table[i].sig;
    }

    return -1;
}

void signal_set(int sig, void (*handler)(int))
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = handler;
    // relay on the default behaviour
    // act.sa_flags = SA_RESTART;
    sigemptyset(&act.sa_mask);

    if (sigaction(sig, &act, NULL) < 0)
        LS_WARNING("sigaction(%d) failed", sig);
}
