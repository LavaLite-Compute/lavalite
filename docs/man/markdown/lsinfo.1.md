\

# NAME

**lsinfo** - displays load sharing configuration information

# SYNOPSIS

**lsinfo** \[**-l**\] \[**-m** \| **-M**\] \[**-r**\] \[**-t**\]
\[*resource_name* \...\]

**lsinfo** \[**-h** \| **-V**\]

# DESCRIPTION

By default, displays all load sharing configuration information
including resource names and their meanings, host types and models, and
associated CPU factors known to the system.

By default, displays information about all resources. Resource
information includes resource name, resource type, description, and the
default sort order for the resource.

You can use resource names in task placement requests.

Use this command with options to selectively view configured resources,
host types, and host models.

# OPTIONS

**-l** 

:   

    Displays resource information in a long multi-line format.
    Additional parameters are displayed including whether a resource is
    built-in or configured, and whether the resource value changes
    dynamically or is static. If the resource value changes dynamically
    then the interval indicates how often it is evaluated.

```{=html}
<!-- -->
```

**-m** 

:   

    Displays only information about host models that exist in the
    cluster.

```{=html}
<!-- -->
```

**-M**

:   

    Displays information about all host models in the file lsf.shared.

```{=html}
<!-- -->
```

**-r** 

:   

    Displays only information about configured resources.

```{=html}
<!-- -->
```

**-t** 

:   

    Displays only information about configured host types. See lsload(1)
    and lshosts(1).

```{=html}
<!-- -->
```

*resource_name* \...

:   

    Displays only information about the specified resources.

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

# SEE ALSO

lsfintro(1), lshosts(1), lsload(1), lsf.shared(5), ls_info(3),
ls_policy(3)
