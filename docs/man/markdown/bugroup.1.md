\

# NAME

**bugroup** - displays information about user groups

# SYNOPSIS

**bugroup **\[**-l**\] \[**-r**\] \[**-w**\] \[*user_group* \...\]

**bugroup **\[**-h** \| **-V**\]

# DESCRIPTION

Displays user groups and user names for each group.

The default is to display information about all user groups.

# OPTIONS

**-l**

:   

    Displays information in a long multi-line format. Also displays
    share distribution if shares are configured.

```{=html}
<!-- -->
```

**-r** 

:   

    Expands the user groups recursively. The expanded list contains only
    user names; it does not contain the names of subgroups. Duplicate
    user names are listed only once.

```{=html}
<!-- -->
```

**-w**

:   

    Wide format. Displays user and user group names without truncating
    fields.

```{=html}
<!-- -->
```

*user_group *\...

:   

    Only displays information about the specified* *user groups.* *Do
    not use quotes when specifying multiple user groups.

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

In the list of users, a name followed by a slash (/) indicates a
subgroup.

# FILES

User groups and user shares are defined in the configuration file
lsb.users(5).

# SEE ALSO

lsb.users(5), bmgroup(1), busers(1)
