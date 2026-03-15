\

# NAME

**lsf.shared**

## Overview

The lsf.shared file contains common definitions that are shared by all
load sharing clusters defined by lsf.cluster.*cluster_name* files. This
includes lists of cluster names, host types, host models, the special
resources available, and external load indices.

## Contents

> · Cluster Section
>
> · HostType Section
>
> · HostModel Section
>
> · Resource Section

# Cluster Section

(Required) Lists the cluster names recognized by the Lava system

# Cluster Section Structure

The first line must contain the mandatory keyword ClusterName. The other
keyword is optional.

Each subsequent line defines one cluster.

# ClusterName

## Description

(Required) Defines all cluster names recognized by the Lava system

All cluster names referenced anywhere in the Lava system must be defined
here. The file names of cluster-specific configuration files must end
with the associated cluster name.

# Servers

## Description

By default, the first ten hosts listed in the Host section of
lsf.cluster.*cluster_name* are available to LIMs in remote clusters.

This parameter is useful when LSF_CONFDIR is not shared or replicated.

# Cluster Section Example

Begin Cluster\
ClusterName Servers\
cluster1 hostA\
cluster2 hostB\
End Cluster

# HostType Section

(Required) Lists the valid host types in the cluster

# HostType Section Structure

The first line consists of the mandatory keyword TYPENAME.

Subsequent lines name valid host types.

# TYPENAME

## Description

Host type names are usually based on a combination of the hardware name
and operating system. If your site already has a system for naming host
types, you can use the same names for Lava.

# HostType Section Example

Begin HostType\
TYPENAME\
SUN41\
SOLSPARC\
ALPHA\
HPPA\
NTX86\
End HostType

# HostModel Section

(Required) Lists models of machines and gives the relative CPU scaling
factor for each model

Lava uses the relative CPU scaling factor to normalize the CPU load
indices so that jobs are more likely to be sent to faster hosts. The CPU
factor affects the calculation of job execution time limits and
accounting. Using large or inaccurate values for the CPU factor can
cause confusing results when CPU time limits or accounting are used.

# HostModel Section Structure

The first line consists of the mandatory keywords MODELNAME, CPUFACTOR,
and ARCHITECTURE.

Subsequent lines define a model and its CPU factor.

# ARCHITECTURE

## Description

(Reserved for system use only) Indicates automatically detected host
models that correspond to the model names.

# CPUFACTOR

## Description

Though it is not required, you would typically assign a CPU factor of
1.0 to the slowest machine model in your system and higher numbers for
the others. For example, for a machine model that executes at twice the
speed of your slowest model, a factor of 2.0 should be assigned.

# MODELNAME

## Description

Generally, you need to identify the distinct host types in your system,
such as MIPS and SPARC first, and then the machine models within each,
such as SparcIPC, Sparc1, Sparc2, and Sparc10.

# HostModel Section Example

Begin HostModel\
MODELNAME CPUFACTOR ARCHITECTURE\
PC400 13.0 (i86pc_400 i686_400)\
PC450 13.2 (i86pc_450 i686_450)\
Sparc5F 3.0 (SUNWSPARCstation5_170_sparc)\
Sparc20 4.7 (SUNWSPARCstation20_151_sparc)\
Ultra5S 10.3 (SUNWUltra5_270_sparcv9 SUNWUltra510_270_sparcv9)\
End HostModel

# Resource Section

(Optional) Defines resources.

# Resource Section Structure

The first line consists of the keywords. RESOURCENAME and DESCRIPTION
are mandatory. The other keywords are optional. Subsequent lines define
resources.

# RESOURCENAME

## Description

The name you assign to the new resource. An arbitrary character string.

> · A resource name cannot begin with a number.
>
> · A resource name cannot contain any of the following characters:
>
> > : . ( ) \[ + - \* / ! & \| \< \> @ =

· A resource name cannot be any of the following reserved names:

> cpu cpuf io logins ls idle maxmem maxswp maxtmp type model status it
> mem ncpus ndisks pg r15m r15s r1m swap swp tmp ut

· Resource names are case sensitive

· Resource names can be up to 29 characters in length

# TYPE

## Description

The type of resource:

> · Boolean\--Resources that have a value of 1 on hosts that have the
> resource and 0 otherwise.
>
> · Numeric\--Resources that take numerical values, such as all the load
> indices, number of processors on a host, or host CPU factor.
>
> · String\-- Resources that take string values, such as host type, host
> model, host status.

## Default

If TYPE is not given, the default type is Boolean.

# DESCRIPTION

## Description

Brief description of the resource.

The information defined here will be returned by the ls_info() API call
or printed out by the **lsinfo** command as an explanation of the
meaning of the resource.

# INCREASING

Applies to numeric resources only.

## Description

If a larger value means greater load, INCREASING should be defined as Y.
If a smaller value means greater load, INCREASING should be defined as
N.

# INTERVAL

Optional. Applies to dynamic resources only.

## Description

Defines the time interval (in seconds) at which the resource is sampled
by the ELIM.

If INTERVAL is defined for a numeric resource, it becomes an external
load index.

## Default

If INTERVAL is not given, the resource is considered static.

# RELEASE

Applies to numeric shared resources only, such as floating licenses.

## Description

Controls whether Lava releases the resource when a job using the
resource is suspended. When a job using a shared resource is suspended,
the resource is held or released by the job depending on the
configuration of this parameter.

Specify N to hold the resource, or specify Y to release the resource.

## Default

Y

# Resource Section Example

Begin Resource\
RESOURCENAME TYPE INTERVAL INCREASING RELEASE DESCRIPTION\
mips Boolean () () () (MIPS architecture)\
dec Boolean () () () (DECStation system)\
sparc Boolean () () () (SUN SPARC)\
bsd Boolean () () () (BSD unix)\
hpux Boolean () () () (HP-UX UNIX)\
aix Boolean () () () (AIX UNIX)\
solaris Boolean () () () (SUN SOLARIS)\
myResource String () () () (MIPS architecture)\
static_sh1 Numeric () N () (static)\
external_1 Numeric 15 Y () (external)\
End Resource
