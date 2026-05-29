---
title: BTOKENS
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

btokens - display token pool usage

# SYNOPSIS

**btokens**

# DESCRIPTION

Displays the current status of all token pools configured in the cluster.
Token pools are floating license counters used to gate job dispatch when
a resource (such as a software license) has a limited number of
concurrent users.

Token pools are defined in llb.queues. Jobs request tokens with
**bsub --pool**.

# OPTIONS

**--help**
:   Print usage to stderr and exit.

**--version**
:   Print version to stderr and exit.

# OUTPUT

Displays a table with the following columns:

**POOL_NAME**
:   Token pool name.

**TOTAL**
:   Total tokens in the pool.

**USED**
:   Tokens currently allocated to running jobs.

**FREE**
:   Available tokens (TOTAL - USED).

# SEE ALSO

**bsub**(1), **bqueues**(1), **llb.queues**(5), **mbd**(8)
