---
title: BGROUP
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bgroup - display host group membership

# SYNOPSIS

**bgroup**

# DESCRIPTION

Displays all host groups defined in the cluster and their member hosts.
Host groups are defined in ll.hosts and can be used in **bsub --machines**
to restrict job placement.

# OPTIONS

**--help**
:   Print usage to stderr and exit.

**--version**
:   Print version to stderr and exit.

# OUTPUT

Displays a table with the following columns:

**GROUP_NAME**
:   Host group name.

**GROUP_MEMBER**
:   Space-separated list of hosts in the group.

# SEE ALSO

**bsub**(1), **bhosts**(1), **ll.hosts**(5)
