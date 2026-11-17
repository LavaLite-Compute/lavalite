---
title: LLSIGN
section: 8
header: LavaLite System Administration
footer: LavaLite
date: 2026
---

# NAME

llsign - sign LavaLite client requests

# SYNOPSIS

**llsign** < *request* > *signed-request*

# DESCRIPTION

**llsign** signs a client request with the client key,
**ll.auth.key**, which users cannot read. It is installed setgid to
the **mbd** user's group, the group that can read the key.

Nobody runs it by hand. The LavaLite library runs it for every request
a command or an application sends to **mbd**: the encoded request on
its standard input, the signed request on its standard output, same
length.

**llsign** writes into the request:

- the real uid and gid of the caller, from the kernel, whatever the
  request says;
- the current time;
- the HMAC-SHA256 of the whole request, header and payload.

It signs only client requests: the list of client operations is
compiled in, any other operation, **sbd** registration and every
daemon message included, is refused. A request larger than 16 MiB is
refused.

So a user can obtain a signed request for their own uid, never for
another uid or for a daemon operation. See AUTHENTICATION in
**lavalite**(7).

# EXIT STATUS

0 on success. 1 on error, with the reason on standard error and
nothing on standard output.

# ENVIRONMENT

**LL_CONF_DIR**
:   Directory with **ll.auth.key**. A key other than the cluster's
    gives requests that **mbd** rejects.

**LL_SIGN**
:   Read by the library, not by **llsign**: the path of **llsign** to
    run instead of the installed one, for tests from a build tree.
    Any other program cannot read the key.

# FILES

*libexec/llsign*
:   Owner **root**, group the **mbd** user's group, mode 2755. Set by
    **post-install.sh**; **make install** resets it, run
    **post-install.sh** again after every install.

*$LL_CONF_DIR*/ll.auth.key
:   Client key, owner **root**, group the **mbd** user's group,
    mode 0640.

# SEE ALSO

**lavalite**(7), **mbd**(8), **sbd**(8), **ll.conf**(5)
