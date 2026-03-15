\

# NAME

**lsrcp** - remotely copies files using Lava

# SYNOPSIS

**lsrcp** \[**-a**\]** ***source_file*** ***target_file*** ** .PP
**lsrcp** \[**-h \| -V**\]

# DESCRIPTION

Remotely copies files using Lava.

lsrcp is an Lava-enabled remote copy program that transfers a single
file between hosts in an Lava cluster. lsrcp uses RES on an Lava host to
transfer files. If Lava is not installed on a host or if RES is not
running then lsrcp uses rcp to copy the file.

To use lsrcp, you must have read access to the file being copied.

Both the source and target file must be owned by the user who issues the
command.

lsrcp uses rcp to copy a source file to a target file owned by another
user. See rcp(1) and LIMITATIONS below for details.

# OPTIONS

**-a**

:   

    Appends *source_file* to *target_file*.

```{=html}
<!-- -->
```

*source_file target_file*

:   

    Specify an existing file on a local or remote host that you want to
    copy, and a file to which you want to copy the source file.

> File format is as follows:

> \[\[*user_name***@**\]\[*host_name*\]**:**\]\[*path***/**\]*file_name*

> *user_name*

> > Login name to be used for accessing files on the remote host. If
> > *user_name* is not specified, the name of the user who issued the
> > command is used.

> *host_name*

> > Name of the remote host on which the file resides. If *host_name* is
> > not specified, the local host, the host from which the command was
> > issued is used.

> *path*

> > Absolute path name or a path name relative to the login directory of
> > the user. Shell file name expansion is not supported on either the
> > local or remote hosts. Only single files can be copied from one host
> > to another.
>
> > Use \"/\" to transfer files from a UNIX host to a UNIX host. For
> > example:
>
> > \% **lsrcp file1 hostD:/home/usr2/test/file2**
> >
> > *file_name*
>
> > > Name of source file. File name expansion is not supported.
>
> **-h**
>
> :   
>
>     Prints command usage to stderr and exits.
>
> ```{=html}
> <!-- -->
> ```
>
> **-V**
>
> :   
>
>     Prints Lava release version to stderr and exits.

# EXAMPLES

\% **lsrcp myfile \@hostC:/home/usr/dir1/otherfile** .PP Copies file
myfile from the local host to file otherfile on hostC.

\% **lsrcp user1@hostA:/home/myfile user1@hostB:otherfile** .PP Copies
the file myfile from hostA to file otherfile on hostB.

\% **lsrcp -a user1@hostD:/home/myfile /dir1/otherfile** .PP Appends the
file myfile on hostD to the file otherfile on the local host.

\% **lsrcp /tmp/myfile user1@hostF:\~/otherfile** .PP Copies the file
myfile from the local host to file otherfile on hostF in user1\'s home
directory.

# SEE ALSO

rsh(1), rcp(1), lsfintro(1), res(8)

# DIAGNOSTICS

lsrcp attempts to copy *source_file* to *target_file* using RES. If RES
is down or fails to copy the *source_file*, lsrcp will use either rsh
when the -a option is specified, or rcp when -a is not specified.

# LIMITATIONS

File transfer using lscrp is not supported in the following contexts:

\- If Lava account mapping is used; lsrcp fails when running under a
different user account

\- On Lava client hosts. Lava client hosts do not run RES, so lsrcp
cannot contact RES on the submission host

\- Third party copies. lsrcp does not support third party copies, when
neither source nor target file are on the local host. In such a case rcp
or rsh will be used. If the *target_file* exists, lsrcp preserves the
modes; otherwise, lsrcp uses the *source_file* modes modified with the
umask (see umask(2)) of the source host.

You can do the following:

rcp on UNIX- if lsrcp cannot contact RES on the submission host, it
attempts to use rcp to copy the file. You must set up the
/etc/hosts.equiv or HOME/.rhosts file in order to use rcp. See the
rcp(1) and rsh(1) manual pages for more information on using the rcp
command.

You can replace lsrcp with your own file transfer mechanism as long as
it supports the same syntax as lsrcp. This might be done to take
advantage of a faster interconnection network, or to overcome
limitations with the existing lsrcp. SBD looks for the lsrcp executable
in the LSF_BINDIR directory as specified in the lsf.conf file.
