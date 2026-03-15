# NAME

lsfbase - Lava Base system.

# DESCRIPTION

Lava Base system is a load sharing software which integrates a
heterogeneous network of computers running UNIX systems. It consists of
the It consists of the Load Information Manager (LIM), the Remote
Execution Server (RES), the Load Sharing LIBrary (LSLIB), and a variety
of load sharing applications and utilities. Lava interoperates on many
UNIX platforms. Some of the load sharing applications (each with its own
man page) are as follows:

**lsfbatch**(1)

:   load sharing batch utility. Distribute parallel as well as
    sequential batch jobs to the hosts in a distributed system for
    execution.

Lava also has a set of commands that can be used as tools to monitor 

:   the status of the Lava cluster, find out the best host, or run jobs
    on the best host. The currently available tools are:

**lseligible**(1)

:   display the remote execution eligibility of a task.

**lshosts**(1)

:   display configuration information about hosts participating in load
    sharing.

**lsid**(1)

:   display the name of the local Lava cluster and the name of its
    master LIM host.

**lsinfo**(1)

:   display load sharing configuration information.

**lsload**(1)

:   display the load information of load sharing hosts.

**lsloadadj**(1)

:   adjust the load condition data of load sharing hosts.

**lsplace**(1)

:   display the currently best host or hosts for executing one or more
    load sharing tasks.

**lsmon**(1)

:   full-screen Lava monitoring utility that displays and updates the
    load information of hosts in the local cluster.

**lsrcp**(1)

:   copy a single file from one host to another.

# SEE ALSO

**lsfintro**(1), **lstools**(1), **lim**(8), **res**(8), **lslib**(3),
**lsf.conf**(5)
