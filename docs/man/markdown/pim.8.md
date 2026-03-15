# NAME

pim - Process Information Manager (PIM) for the Load Sharing Facility
(LSF) system

# SYNOPSIS

**LSF_SERVDIR/pim \[ -h \] \[ -V \] \[ -d ***env_dir*** \] \[
-***debug_level*** \]**

# DESCRIPTION

PIM is a daemon started by the **LIM**(8) on every server host which is
participating in load sharing. The PIM collects resource usage of the
processes running on the local host. The information collected by the
PIM is used by other parts of LSF (e.g., **sbatchd**(8)) to monitor
resource consumption and enforce usage limits.

The PIM updates the process information every 15 minutes unless an
application queries this information. If an application requests the
information, the PIM will update the process information every
**LSF_PIM_SLEEPTIME** seconds, where **LSF_PIM_SLEEPTIME** can be
defined in the **lsf.conf**(5) file. The default value for
**LSF_PIM_SLEEPTIME** is 15 seconds, if this parameter is not defined.
If the information is not queried by any application for more than 5
minutes, the PIM will revert back to the 15 minute update period.

The process information is stored in
**LSF_PIM_INFODIR/pim.info.\<hostname\>** where **LSF_PIM_INFODIR** can
be defined in the **lsf.conf** file. If this parameter is not defined,
the default directory is **/tmp**. The PIM also reads this file when it
starts up so that it can accumulate the resource usage of dead processes
for existing process groups.

# OPTIONS

**-h**

:   Print command usage to stderr and exit.

**-V**

:   Print LSF release version to stderr and exit.

**-d *env_dir***

:   Read **lsf.conf** from the directory *env_dir,* rather than the
    default directory **/etc**, or the directory specified by the
    **LSF_ENVDIR** environment variable.

**-***debug_level*

:   Set the debug level. Valid values are 1 and 2. If specified, PIM
    runs in debugging mode. If *debug_level* is 1, PIM runs in the
    background, with no associated control terminal. If *debug_level* is
    2, PIM runs in the foreground, printing error messages on to tty.
    The *debug_level* option overrides the environment variable
    **LSF_LIM_DEBUG** defined in **lsf.conf**(5).

# NOTE

PIM needs read access to **/dev/kmem** or its equivalent.

# FILES

**/etc/lsf.conf** (by default) or **LSF_ENVDIR/lsf.conf**

:   

# SEE ALSO

**lsf.conf**(5)
