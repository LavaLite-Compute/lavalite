\

# NAME

**lseligible** - displays whether a task is eligible for remote
execution

# SYNOPSIS

**lseligible** \[**-r**\] \[**-q**\] \[**-s**\] *task*

**lseligible** \[**-h** \| **-V**\]

# DESCRIPTION

Displays whether the specified task is eligible for remote execution.

By default, only tasks in the remote task list are considered eligible
for remote execution.

# OPTIONS

**-r**

:   

    Remote mode. Considers eligible for remote execution any task not
    included in the local task list.

```{=html}
<!-- -->
```

**-q** 

:   

    Quiet mode. Displays only the resource requirement string defined
    for the task. The string ELIGIBLE or NON-ELIGIBLE is omitted.

```{=html}
<!-- -->
```

**-s**

:   

    Silent mode. No output is produced. The -q and -s options are useful
    for shell scripts which operate by testing the exit status (see
    DIAGNOSTICS).

```{=html}
<!-- -->
```

*task*

:   

    Specify a command.

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

If the task is eligible, the string ELIGIBLE followed by the resource
requirements associated with the task are printed to stdout. Otherwise,
the string NON-ELIGIBLE is printed to stdout.

If lseligible prints ELIGIBLE with no resource requirements, the task
has the default requirements of CPU consumption and memory usage.

# SEE ALSO

ls_eligible(3)

# DIAGNOSTICS

lseligible has the following exit statuses:

0 Task is eligible for remote execution

1 Command is to be executed locally

-1 Syntax errors

-10 A failure is detected in the Lava system
