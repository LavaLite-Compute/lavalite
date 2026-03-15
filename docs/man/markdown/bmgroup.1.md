\

# NAME

**bmgroup** - displays information about host groups

# SYNOPSIS

**bmgroup ** \[**-w**\] \[*host_group *\...\]

**bmgroup **\[**-h** \| **-V**\]

# DESCRIPTION

Displays host groups and host names for each group.

By default, displays information about all host groups.

# OPTIONS

**-w**

:   

    Wide format. Displays host and host group names without truncating
    fields.

```{=html}
<!-- -->
```

*host_group *\...

:   

    Only displays information about the specified host groups*. *Do not
    use quotes when specifying multiple host groups.

```{=html}
<!-- -->
```

**-h**

:   

    Prints command usage to stderr and exits.

```{=html}
<!-- -->
```

**-V**

:   

    Prints Lava release version to stderr and exits.

# OUTPUT

In the list of hosts, a name followed by a slash (/) indicates a
subgroup.

# FILES

Host groups are defined in the configuration file lsb.hosts(5).

# SEE ALSO

lsb.hosts(5), bugroup(1), bhosts(1)
