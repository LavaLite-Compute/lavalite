# NAME

lsf - load sharing facility

# DESCRIPTION

Lava, or Load Sharing Facility, is a load sharing and distributed batch
queueing software that integrates a heterogeneous network of computers
running UNIX systems. It consists of the following components:

**Lava Base:**

:   The software upon which all the other components are based. It
    includes two servers, the Load Information Manager (LIM) and the
    Remote Execution Server (RES), API for accessing services provided
    by the Lava Base system, Base software services and other load
    sharing tools. (see **lsfbase**(1)).

**Lava Batch:**

:   The set of utilities providing batch job scheduling for distributed
    and heterogeneous environments, ensuring optimal resource sharing.
    It includes the master and slave batch daemons (mbatchd and sbatchd)
    and API for accessing services provided by the Lava Batch system
    (see **lsfbatch**(1)).

# RESOURCE REQUIREMENT STRINGS

Many of the above commands and utilities permit a resource requirement
string to be specified. A resource requirement string contains
information used for querying for information from the LIM about hosts
or requesting task placement decisions.

A resource requirement string is divided into four sections including a
selection section, an ordering section, a resource usage section and a
job spanning section. The selection section specifies the criteria for
selecting hosts from the system. The ordering section indicates how the
hosts which meet the selection criteria should be sorted. The resource
usage section specifies the expected resource consumption of the task or
the resource reservation for a batch job. The job spanning section
specifies whether to span a (parallel) batch job across multiple hosts.
The syntax of a resource requirement expression is

** \"select\[ ***selectstring*** \] order\[ ***orderstring* \]

**rusage\[ ***usagestring*** \] span\[ ***spanstring* \]\"

where \`**select**\', \`**order**\' , \`**rusage**\' and \`**span**\'
are the section names. Any character in the resource requirement
expression not within the above sections are ignored. If a section is
repeated multiple times in a resource requirement expression, then only
the first occurrence is considered. The syntax for each of
\`*selectstring*\', \`*orderstring*\', \`*usagestring*\' and
\`*spanstring*\' is defined below. Depending on the command, one or more
of these sections may be ignored. For example, **lshosts**(1) will only
select hosts, but not order them, **lsload**(1) will select and order
the hosts, **lsplace**(1) uses the information in select, order and
rusage sections to select an appropriate host for a task.
**lsloadadj**(1) uses the resource usage section to determine how the
load information should be adjusted on a host, while **bsub**(1) uses
all the four sections. Sections other than these are ignored. If no
section name is given, then the string is treated as a
\`*selectstring*\'. The \`**select**\' keyword may be omitted if the
\`*selectstring*\' appears as the first string in the resource
requirement.

## Selection String

The selection string specifies the characteristics a host must have to
be returned. It is a logical expression built from a set of resource
names. The resource names and their descriptions can be obtained by
running the Lava utility program **lsinfo**(1). The resource names
\`**swap**\', \`**idle**\', \`**login**\', and \`**cpu**\' are aliases
for \`**swp**\', \`**it**\', \`**ls**\' and \`**r1m**\' respectively
which are returned by **lsinfo**(1).

Resource names correspond to information maintained by the LIM about
hosts. Some resources correspond to dynamic information about a host,
such as its CPU queue length, available memory, and available swap
space. These resources are referred to as load indices and can be
retrieved via **lsload**(1). Other resources correspond to static
information about a host such as its type, host model, relative CPU
speed, total memory and total swap space. This information can be
retrieved via **lshosts**(1). The system administrator can define other
resources in the system in addition to those built in to LIM.

An arbitrary expression with resource names being combined with logical
or mathematical operators, and functions can be specified. Valid
operators include \`**&&**\' (logical AND), \`**\|\|**\' (logical OR),
\`**!**\' (logical NOT), \`**+**\' (addition), \`**-**\' (subtraction),
\`**\***\',(multiplication) and \`**/**\' (division). The function,
**defined**, can be used to determine the hosts which have a particular
resource defined. The selection expression is evaluated for each host;
if the result is non-zero, then that host is selected.

For example,

**\"select\[ (swp\>50 && mem\>=10 && type==MIPS) \|\| (swp\>35 &&
type==ALPHA) \]\"**

**\"select\[ ((2\*r15s + 3\*r1m + r15m)/6 \< 1.0) && !fs && (cpuf \>
4.0) \]\"**

**\"select\[ defined(verilog_license) \]\"**

are valid selection expressions. Resource names which are of the boolean
type (e.g. \`**fs**\' for a file server resource) have a value of 1 if
they are defined for a host, and 0 otherwise. The default is to select
all configured hosts in the cluster. For the string valued resources
\`**type**\' and \`**model**\', special values of \`**any**\' and
\`**local**\' can be used to select any value or the same value as that
of the local host, respectively. For example, \"**type==local**\" would
select hosts of the same type as the local host. When the run queue
lengths \`**r15s**\', \`**r1m**\', or \`**r15m**\' are specified, the
effective value of the queue length is used.

For tasks where an arbitrary selection string is not required a
restricted syntax is provided. The restricted syntax allows for
combining resource names using \`:\' (logical AND) and \`-\' (logical
NOT). For example, \"**r15m=1.5:mem=20:swp=12:-ultrix**\" is a valid
selection string in the restricted syntax. It is equivalent to \"**r15m
\<= 1.5 && mem \>= 20 && swp \>= 12 && !ultrix**\" in the unrestricted
syntax. A selection string in the restricted syntax is of the form

\"*res*\[=*value*\]:*res*\[=*value*\]: \... :*res*\[=*value*\]\"

where each \`*res*\' is resource name. *value* may only be specified for
resources whose value type is numeric or string. The semantics of \`=\'
depends on the value type and sorting order for the resource. For string
resources \`=\', is equivalent to \`==\'. For numeric valued resources
\`=\' is equivalent to \`\>=\' (\`\<=\') if the sorting order for the
resource is decreasing (increasing). A \`-\' may only be used in front
of a boolean resource name or in isolation. In isolation \`-\' is
equivalent to \"**type==any**\". If the value is not given for a numeric
resource then it is equivalent to saying that the resource must have a
non-zero value. Other examples of a selection string in the restricted
syntax are, \"**-:swp=12**\", \"**type=MIPS:maxmem=20**\", and
\"**status=busy**\".

## Order String

The order string allows the selected hosts to be sorted according to the
value(s) of resource(s). The syntax of the order string is

\"\[-\]*res*:\[-\]*res*:\...\[-\]*res*\"

Each \`*res*\' is a resource name with a numeric value type. Currently
only load indices such as \`**mem**\', \`**swp**\', and \`**tmp**\'
which are returned by **lsload**(1) are considered for sorting. For
example, \"**swp:r1m:tmp:r15s**\" is a valid order string. The order
string is used as input to a multi-level sorting algorithm, where each
sorting phase orders the hosts according to one particular load index
and discards some hosts. The remaining hosts are passed onto the next
phase. The first phase begins with the last index and proceeds from
right to left. The final phase of sorting orders the hosts according to
their status, with hosts that are currently not available for load
sharing (i.e., not in the **ok** state) listed at the end. When sorting
is done on the particular index, the direction in which the hosts are
sorted (increasing vs. decreasing values) is determined by the default
order returned by **lsinfo**(1) for that index. This direction is chosen
such that after sorting, the hosts are ordered from best to worst on
that index. A \`-\' before the index name reverses the order.

If no sorting string is specified, the default sorting string is
\"**r15s:pg**\".

When the run queue lengths \`**r15s**\', \`**r1m**\', or \`**r15m**\'
are specified, the normalized value of the queue length is used when
sorting.

## Resource Usage String

The resource usage section is used to specify the resource reservation
for batch jobs via **bsub -R** option and queue configuration parameter
**RES_REQ.** External indices are also considered in the resource usage
string for this purpose. The syntax of the resource usage string is

\"*res*=*value*:*res*=*value*: \...
:*res*=*value*\[:*duration=value*\]\[:*decay*=*value*\]\"

The \"*res*=*value*\" is used to specify the amount of resource to be
reserved for a job after the job starts. If *value* is not specified,
the resource will not be reserved. \"*duration=value*\" and
\"*decay=value*\" are optionally used to specify how long the resource
reservation will be in effect and how the reserved amount of resource is
decreased as the time passes. \"*duration*\" and \"*decay*\" are
keywords.

The value of \"*duration*\" (in minutes) is the time period within which
the specified resources will be reserved. The value can be specified in
hours if followed by \"h\", e.g., \"*duration=2h*\". If \"*duration*\"
is not given, the default is to reserve the total amount for the
lifetime of the job.

A value of 1 for \"*decay*\" indicates that the system should linearly
decrease the reserved amount over the duration. A value of 0 causes the
total amount to be reserved for the entire duration or until the job
finishes. All other values for \"*decay*\" are not supported. The
\"*decay*\" keyword is ignored if the duration is not specified. The
default value for \"*decay*\" is 0.

For example, \"*rusage\[mem=50:duration=100:decay=1\]*\" will initially
reserve 50 MBytes of memory. As the job runs, the amount reserved amount
will decrease by 0.5 Mbytes each minute such that the reserved amount is
0 after 100 minutes.

The resource usage string is also used in adjusting the load and for
mapping tasks onto hosts during a placement decision (see **lsplace**(1)
and **lsloadadj**(1)). External indices are not considered in the
resource usage string for this purpose. The syntax of the resource usage
string is \"*res*\[=*value*\]:*res*\[=*value*\]: \...
:*res*\[=*value*\]\" where \`*res*\' is one of the resources whose value
is returned by **lsload**(1). For example, \"**r1m=0.5:mem=20:swp=40**\"
indicates that the task is expected to increase the 1-minute run queue
length by 0.5, consume 20 Mbytes of memory and 40 Mbytes of swap space.
If no value is specified, the task is assumed to be intensive in using
that resource. In this case no more than one task will be assigned to a
host regardless of how many CPUs it has.

The default resource usage for a task is assumed to be
\"**r15s=1.0:r1m=1.0:r15m=1.0**\" which indicates a CPU intensive task
which consumes few other resources.

## Job Spanning String

This string specifies the locality of a parallel batch job. Currently
only the following two cases are supported: \"*span\[hosts=1\]*\"
indicates that all the processors allocated to this job must be on the
same host, while \"*span\[ptile=N\]*\" indicates that up *N*
processor(s) on each host should be allocated to the job.

# RUN QUEUE LENGTHS

The raw CPU queue length is collected by the LIM from the kernel of the
host operating system every 5 seconds. This number represents the total
number of processes that are contending for the CPU(s) on the host. The
raw queue length is averaged over 15 seconds, 1 minute, and 15 minutes
to produce the \`**r15s**\', \`**r1m**\', and \`**r15m**\' load indices,
respectively. The raw queue lengths can be viewed using **lsload**(1).

In order to compare queue lengths on hosts having different numbers of
CPUs and relative CPU speeds, two variations of the raw queue length are
defined. The effective queue length attempts to account for
multiprocessor hosts by considering the number of CPUs. The effective
queue length is calculated by taking the multiprocessor\'s multitasking
feature into consideration such that even if many of the processors are
busy, the host\'s effective queue length may appear to be as good as an
idle uniprocessor (as long as there is one or more idle processors). The
effective queue length is the same as the raw queue length on
uniprocessor hosts. Effective queue lengths are listed when using the
**-E** option of **lsload.** The effective queue length is used by LIM
when testing whether the host has exceeded its busy thresholds. When
\`**r15s**\', \`**r1m**\', or \`**r15m**\' are specified in the
selection section of resource requirement strings, they refer to the
effective queue length. It is also used by **lsfbatch**(1) when
comparing the values specified for queue and host thresholds against the
current load.

The normalized queue length is used by the LIM when making placement
decision about where to send a job (see **lsplace**(1)). It considers
both the number of CPUs and the CPU factor of a host. This is also the
value returned by **lsload** when using the **-N** option. The
normalized queue length attempts to estimate what the load would be on a
host if an additional CPU bound job was dispatched to that host.

# LSF_JOB_STARTER

Users can define the environment variable, LSF_JOB_STARTER, to specify a
job starter command for executing remote tasks. The task\'s arguments
are passed as the arguments to the job starter command. An example use
of the job starter is to specify that the remote task is to run under
**csh**(1). The LSF_JOB_STARTER variable is set to \"/bin/csh -c\" for
this example.

# NOTES

If lsf.conf (see **lsf**(5)) is not in the default **/etc** directory,
set the environment variable **LSF_ENVDIR** to the name of the directory
where lsf.conf is stored.

# SEE ALSO

**lsf.conf**(5), **lim**(8), **res**(8), **nios**(8), **lslib**(3),
