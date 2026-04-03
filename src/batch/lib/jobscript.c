/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#include "base/lib/ll.bufsiz.h"
#include "batch/lib/wire.h"

/* Generate:
 *
 #!/bin/sh

 # LavaLite: environment
 SHELL='/bin/sh'; export SHELL
 HISTSIZE='1000'; export HISTSIZE
 LANG='en_US.UTF-8'; export LANG
 LOGNAME='david'; export LOGNAME
 PATH='/opt/lavalite/bin:/usr/local/bin:/usr/bin:/bin'; export PATH
 LD_LIBRARY_PATH='/opt/lavalite/lib'; export LD_LIBRARY_PATH
 MODULEPATH='/etc/modulefiles:/usr/share/modulefiles'; export MODULEPATH
 LOADEDMODULES='lavalite/lavalite'; export LOADEDMODULES
 LMOD_VERSION='8.7.65'; export LMOD_VERSION
 LMOD_DIR='/usr/share/lmod/lmod/libexec'; export LMOD_DIR
 MAIL='/var/spool/mail/david'; export MAIL
 # LavaLite: end environment

 # LavaLite: user command
 /opt/jobs/simulate.sh --nodes 4 --tasks 16

 ExitStat=$?
 echo "$ExitStat $(date +%s)" > "$LL_JOBDIR/exit"
 exit $ExitStat
*/

/*
 * Growing byte buffer — heap only, no fixed limit.
 */
struct ll_buf {
    char   *data;
    size_t  len;
    size_t  cap;
};

static int ll_buf_grow(struct ll_buf *b, size_t need)
{
    size_t want;
    char *p;

    if (b->cap - b->len >= need)
        return 0;

    if (need > SIZE_MAX - b->len)
        return -1;

    want = b->cap ? b->cap : LL_BUFSIZ_4K;
    while (want < b->len + need) {
        if (want > SIZE_MAX - LL_BUFSIZ_4K)
            return -1;
        want += LL_BUFSIZ_4K;
    }

    p = realloc(b->data, want);
    if (p == NULL)
        return -1;

    b->data = p;
    b->cap  = want;
    return 0;
}

static int ll_buf_append_mem(struct ll_buf *b, const void *p, size_t n)
{
    if (ll_buf_grow(b, n + 1) < 0)
        return -1;

    memcpy(b->data + b->len, p, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static int ll_buf_append_str(struct ll_buf *b, const char *s)
{
    return ll_buf_append_mem(b, s, strlen(s));
}

/*
 * Emit: NAME='<value>'; export NAME\n
 * Single-quotes the value; escapes embedded single quotes as '\''.
 * Value must not contain newlines (enforced by caller).
 */
static int ll_buf_append_sh_export(struct ll_buf *b, const char *name,
                                   size_t nlen, const char *value)
{
    const char *p;

    if (ll_buf_append_mem(b, name, nlen) < 0)
        return -1;
    if (ll_buf_append_str(b, "='") < 0)
        return -1;

    if (value == NULL)
        value = "";

    for (p = value; *p != '\0'; p++) {
        if (*p == '\'') {
            if (ll_buf_append_str(b, "'\\''") < 0)
                return -1;
            continue;
        }
        if (ll_buf_append_mem(b, p, 1) < 0)
            return -1;
    }

    if (ll_buf_append_str(b, "'; export ") < 0)
        return -1;
    if (ll_buf_append_mem(b, name, nlen) < 0)
        return -1;
    if (ll_buf_append_str(b, "\n") < 0)
        return -1;

    return 0;
}

static int is_alpha(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static int is_sh_ident(const char *s, size_t n)
{
    size_t i;

    if (s == NULL || n == 0)
        return 0;
    if (!is_alpha((unsigned char)s[0]) && s[0] != '_')
        return 0;
    for (i = 1; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!is_alpha(c) && !is_digit(c) && c != '_')
            return 0;
    }
    return 1;
}

static int is_bash_func(const char *name, size_t n)
{
    /* BASH_FUNC_<fname>%% */
    if (n < 12)
        return 0;
    if (strncmp(name, "BASH_FUNC_", 10) != 0)
        return 0;
    if (name[n - 2] != '%' || name[n - 1] != '%')
        return 0;
    return 1;
}

/*
 * env_deny_table and env_is_denied must come before create_jobscript.
 */

struct env_rule {
    const char *name;
    size_t      len;
    int         prefix;
};

static const struct env_rule env_deny_table[] = {
    /* lmod session state */
    { "LMOD_",                           5,  1 },
    { "MODULEPATH",                     10,  1 },
    { "LOADEDMODULES",                  13,  0 },
    { "MODULESHOME",                    11,  0 },
    { "_LMFILES_",                       9,  0 },
    { "FPATH",                           5,  0 },
    { "BASH_ENV",                        8,  0 },
    { "__LMOD_REF_COUNT_",              17,  1 },
    { "_ModuleTable",                   12,  1 },

    /* scheduler internals */
    { "LL_JOB",                          6,  1 },
    { "LL_QUEUE",                        8,  1 },
    { "LL_HOSTS",                        8,  1 },

    /* always reset by sbd on execution host */
    { "HOME",                            4,  0 },
    { "PWD",                             3,  0 },
    { "USER",                            4,  0 },

    /* terminal */
    { "TERM",                            4,  0 },
    { "TERMCAP",                         7,  0 },
    { "LINES",                           5,  0 },
    { "COLUMNS",                         7,  0 },

    /* display / desktop session */
    { "DISPLAY",                         7,  0 },
    { "WAYLAND_DISPLAY",                15,  0 },
    { "XAUTHORITY",                     10,  0 },
    { "DBUS_SESSION_BUS_ADDRESS",       24,  0 },
    { "SESSION_MANAGER",                15,  0 },
    { "WINDOWID",                        8,  0 },
    { "XMODIFIERS",                     10,  0 },
    { "GTK_MODULES",                    11,  0 },
    { "QT_IM_MODULE",                   12,  0 },
    { "QT_ACCESSIBILITY",               16,  0 },
    { "IM_CONFIG_CHECK_ENV",            19,  0 },
    { "IM_CONFIG_PHASE",                15,  0 },
    { "GDMSESSION",                     10,  0 },
    { "DESKTOP_SESSION",                15,  0 },
    { "XDG_",                            4,  1 },
    { "GNOME_",                          6,  1 },
    { "GJS_",                            4,  1 },

    /* systemd session */
    { "JOURNAL_STREAM",                 14,  0 },
    { "INVOCATION_ID",                  13,  0 },
    { "SYSTEMD_EXEC_PID",               16,  0 },
    { "MANAGERPID",                     10,  0 },
    { "MEMORY_PRESSURE_WRITE",          21,  0 },
    { "MEMORY_PRESSURE_WATCH",          21,  0 },

    /* shell state */
    { "PS1",                             3,  0 },
    { "SHLVL",                           5,  0 },
    { "OLDPWD",                          6,  0 },
    { "_",                               1,  0 },

    /* xterm */
    { "XTERM_VERSION",                  13,  0 },
    { "XTERM_SHELL",                    11,  0 },
    { "XTERM_LOCALE",                   12,  0 },

    /* misc interactive noise */
    { "PAGER",                           5,  0 },
    { "LS_COLORS",                       9,  0 },
    { "CLUTTER_DISABLE_MIPMAPPED_TEXT",  30,  0 },
    { "GSM_SKIP_SSH_AGENT_WORKAROUND",   29,  0 },
    { "INSIDE_EMACS",                   12,  0 },
    { "EDITOR",                          6,  0 },
    { "SSH_AUTH_SOCK",                  13,  0 },

    /* build env */
    { "CFLAGS",                          6,  0 },
    { "CPPFLAGS",                        8,  0 },
    { "PKG_CONFIG",                     10,  0 },
    { "PKG_CONFIG_PATH",                15,  0 },
    { "DEBUGINFOD_URLS",                15,  0 },
    { "MANPATH",                         7,  0 },

    { NULL, 0, 0 }
};

static int rule_exact(const char *name, size_t n, const char *rule, size_t rlen)
{
    if (n != rlen)
        return 0;
    return strncmp(name, rule, n) == 0;
}

static int rule_prefix(const char *name, size_t n, const char *rule, size_t rlen)
{
    if (n < rlen)
        return 0;
    return strncmp(name, rule, rlen) == 0;
}

static int env_is_denied(const char *name, size_t n)
{
    const struct env_rule *r;

    for (r = env_deny_table; r->name != NULL; r++) {
        if (r->prefix) {
            if (rule_prefix(name, n, r->name, r->len))
                return 1;
            continue;
        }
        if (rule_exact(name, n, r->name, r->len))
            return 1;
    }

    return 0;
}

/*
 * Script sections — written as functions mirroring ll_buf_append_exit_tail.
 * No macros, no legacy LSBATCH strings.
 */

static int ll_buf_append_shebang(struct ll_buf *b)
{
    return ll_buf_append_str(b, "#!/bin/sh\n");
}

static int ll_buf_append_env_start(struct ll_buf *b)
{
    return ll_buf_append_str(b, "\n# LavaLite: environment\n");
}

static int ll_buf_append_env_end(struct ll_buf *b)
{
    return ll_buf_append_str(b, "# LavaLite: end environment\n");
}

static int ll_buf_append_cmd_start(struct ll_buf *b)
{
    return ll_buf_append_str(b, "\n# LavaLite: user command\n");
}

static int ll_buf_append_exit_tail(struct ll_buf *b)
{
    if (ll_buf_append_str(b, "\nExitStat=$?\n") < 0)
        return -1;
    if (ll_buf_append_str(b,
            "echo \"$ExitStat $(date +%s)\" > \"$LL_JOBDIR/exit\"\n") < 0)
        return -1;
    if (ll_buf_append_str(b, "exit $ExitStat\n") < 0)
        return -1;
    return 0;
}

/*
 * create_jobscript - build the job script sent to sbd.
 *
 * On success: script->data is heap-allocated (caller must free), script->len set.
 * On error:   returns -1.
 */
int create_jobscript(const struct job_submit *js, struct wire_job_script *script)
{
    struct ll_buf b;
    char **ep;

    memset(&b, 0, sizeof(b));

    if (ll_buf_append_shebang(&b) < 0)
        goto oom;
    if (ll_buf_append_env_start(&b) < 0)
        goto oom;

    for (ep = environ; ep != NULL && *ep != NULL; ep++) {
        const char *e    = *ep;
        const char *eq   = strchr(e, '=');
        const char *name;
        const char *val;
        size_t      nlen;

        if (eq == NULL)
            continue;

        name = e;
        nlen = (size_t)(eq - e);
        val  = eq + 1;

        if (!is_sh_ident(name, nlen))
            continue;
        if (is_bash_func(name, nlen))
            continue;
        if (env_is_denied(name, nlen))
            continue;
        if (strchr(val, '\n') != NULL)
            continue;

        if (ll_buf_append_sh_export(&b, name, nlen, val) < 0)
            goto oom;
    }

    if (ll_buf_append_env_end(&b) < 0)
        goto oom;
    if (ll_buf_append_cmd_start(&b) < 0)
        goto oom;
    if (ll_buf_append_str(&b, js->command) < 0)
        goto oom;
    if (ll_buf_append_exit_tail(&b) < 0)
        goto oom;

    script->data = b.data;
    script->len  = (uint32_t)b.len;
    return 0;

oom:
    free(b.data);
    return -1;
}
