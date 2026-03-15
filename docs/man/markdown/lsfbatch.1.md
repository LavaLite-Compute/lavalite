# NAME

lsfbatch - Lava Batch system

# DESCRIPTION

Lava Batch is a load sharing batch system that uses LSF Base and LSLIB
to provide distributed batch job scheduling services. It is supported by
master and slave batch daemons that run on hosts that act as servers of
distributed batch jobs (see **mbatchd**(8)). The Batch commands (each
with its own man page) are as follows:

**bsub**(1)

:   submit a job for batched execution.

**bmod**(1)

:   modify the parameters of a submitted job.

**bjobs**(1)

:   display the status and other information about batch jobs.

**bqueues**(1)

:   display the status and other information about batch job queues.

**bhosts**(1)

:   display the status and other information about Batch server hosts.

**busers**(1)

:   display information about Batch users.

**bugroup**(1)

:   display the user group names and their memberships as defined in the
    Batch system.

**bmgroup**(1)

:   display the host group names and their memberships that are defined
    in the Batch system.

**bparams**(1)

:   display the information about the configurable system parameters of
    Batch.

**bpeek**(1)

:   display the stdout and stderr output produced so far by a batch job
    that is being executed.

**bhist**(1)

:   display the processing history of batch jobs.

**bkill**(1)

:   send a UNIX signal to batch jobs.

**bstop**(1)

:   suspend batch jobs.

**bresume**(1)

:   resume suspended batch jobs.

**bchkpnt**(1)

:   checkpoint batch jobs.

**brestart**(1)

:   restart a job from checkpoint its files.

**bmig**(1)

:   migrate a job.

**bswitch**(1)

:   switch pending jobs from one queue to another.

**btop**(1)

:   move a pending job to the top (beginning) of its queue.

**bbot**(1)

:   move a pending job to the bottom (end) of its queue.

**bacct**(1)

:   generate accounting information about batch jobs.

**brun**(1)

:   force a batch job to run.

# ENVIRONMENT

Like other load sharing utilities, Batch needs access to the
**lsf.conf**(5) file to get information about the system configuration.
By default, all commands look to **/etc** to find **lsf.conf**(5),
unless the environment variable **LSF_ENVDIR** is defined. In this case
commands look to the **LSF_ENVDIR** directory to find **lsf.conf**(5).
It is required that all Batch commands use the same **lsf.conf** file as
the Lava daemons.

# SEE ALSO

**mbatchd**(8), **lsf.conf**(5), **lsb.queues**(5), **lsb.hosts**(5),
**lsb.users**(5), **lsb.params**(5)
