\

# NAME

**lsid** - displays the current Lava version number, the cluster name,
and the master host name

# SYNOPSIS

**lsid** \[**-h** \| **-V**\]

# DESCRIPTION

Displays the current Lava version number, the cluster name, and the
master host name.

The master host is dynamically selected from all hosts in the cluster.

# OPTIONS

**-h**

:   

    Prints command usage to stderr and exits.

```{=html}
<!-- -->
```

**-V**

:   

    Prints Lava release version to stderr and exits.

# FILES

The host names and cluster name are defined in
lsf.cluster.*cluster_name* and lsf.shared, respectively.

# SEE ALSO

ls_getclustername(3), ls_getmastername(3), lsinfo(1)
