\

# NAME

**bmod** - modifies job submission options of a job

# SYNOPSIS

**bmod **\[*bsub options*\] \[*job_ID*** \|
\"***job_ID***\[***index***\]\"**\]

**bmod** \[**-h** \| **-V**\]

# OPTION LIST

\
\[**-B** \| **-Bn**\]\
\[**-N** \| **-Nn**\]\
\[**-r **\| **-rn** \]\
\[**-x** \| **-xn**\]\
\[**-b ***begin_time* \| **-bn**\]\
\[**-C ***core_limit* \| **-Cn**\]\
\[**-c** \[*hour***:**\]*minute*\[**/***host_name* \|
**/***host_model*\] \| **-cn**\]\
\[**-D ***data_limit* \| **-Dn**\]\
\[**-e ***err_file* \| **-en**\]\
\[**-E \"***pre_exec_command *\[*argument *\...\]**\"** \| **-En**\]\
\[**-f \"***local_file op *\[*remote_file*\]**\" **\... \| **-fn**\]\
\[**-F** *file_limit* \| **-Fn**\]\
\[**-G ***user_group* \| **-Gn**\]\
\[**-i ***input_file* \| **-in** \| **-is** *input_file * \| **-isn**\]\
\[**-J ***job_name* \| **-J \"%***job_limit***\"** \| **-Jn**\]\
\[**-k ***checkpoint_dir* \| **-k \"***checkpoint_dir***
**\[*checkpoint_period*\]**\"** \| **-kn**\]\
\[**-L ***login_shell* \| **-Ln**\]** ** .br \[**-m
\"***host_name*\[**+**\[*pref_level*\]\] \|
*host_group*\[**+**\[*pref_level*\]\] \...**\"** \| **-mn**\]\
\[**-M ***mem_limit* \| **-Mn**\]\
\[**-n** *num_processors* \| **-nn **\]\
\[**-o ***out_file* \| **-on**\]\
\[**-P** *project_name* \| **-Pn**\]\
\[**-p** *process_limit* \| **-Pn**\]\
\[**-q \"***queue_name \...***\"** \| **-qn**\]\
\[**-R \"***res_req***\"** \| **-Rn**\]\
\[**-sp** *priority* \| **-spn**\]\
\[**-S ***stack_limit* \| **-Sn**\]\
\[**-t ***term_time* \| **-tn**\]\
\[**-u ***mail_user* \| **-un**\]\
\[**-w** **\'***dependency_expression***\'** \| **-wn**\]\
\[**-W*** run_limit *\[/*host_name* \| /*host_model*\] \| **-Wn**\]\
\[**-Z \"***new_command***\"** \| **-Zs \"***new_command***\"** \|
**-Zsn**\]\
\[*job_ID* \| **\"***job_ID***\[***index***\]\"**\]** ** .br \[**-h**\]\
\[**-V**\]

# DESCRIPTION

Modifies the options of a previously submitted job. See bsub(1) for
complete descriptions of job submission options you can modify with
bmod.

Only the owner of the job, or an Lava administrator, can modify the
options of a job.

All options specified at submission time may be changed. The value for
each option may be overridden with a new value by specifying the option
as in bsub. To reset an option to its default value, use the option
string followed by \'n\'. Do not specify an option value when resetting
an option.

The -i, -in, and -Z options have counterparts that support spooling of
input and job command files (-is, -isn, -Zs, and -Zsn).

You can modify all options of a pending job, even if the corresponding
bsub option was not specified.

By default, you can modify resource reservation for running jobs (**-R**
**\"***res_req***\"**). To modify additional job options for running
jobs, define LSB_MOD_ALL_JOBS=Y in lsf.conf.

The following are the only **bmod** options that are valid for running
jobs. You cannot make any other modifications after a job has been
dispatched.

\- Resource reservation (**-R** **\"***res_req***\"**)

\- CPU limit (**-c **\[*hour***:**\]*minute*\[**/***host_name* \|
**/***host_model*\])

\- Memory limit (**-M** *mem_limit*)

\- Run limit (**-W** *run_limit*\[**/***host_name* \|
**/***host_model*\])

\- Standard output file name (**-o** *output_file*)

\- Standard error file name (**-e** *error_file*)

\- Rerunnable jobs (**-r** \| **-rn**)

Modified resource limits cannot exceed the resource limits defined in
the queue.

To modify the CPU limit or the memory limit of running jobs, the
parameters LSB_JOB_CPULIMIT=Y and LSB_JOB_MEMLIMIT=Y must be defined in
lsf.conf.

If you want to specify array dependency by array name, set
JOB_DEP_LAST_SUB in lsb.params. If you do not have this parameter set,
the job will be rejected if one of your previous arrays has the same
name but a different index.

# OPTIONS

*job_ID*** **\|** \"***job_ID***\[***index***\]\"**

:   

    Modifies jobs with the specified job ID.

> Modifies job array elements specified by
> **\"***job_ID***\[***index***\]\"**.

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

bsub(1), lsfbatch(1)

# LIMITATIONS

Modifying the -q option of a job array is not permitted.

If you do not specify **-e** before the job is dispatched, you cannot
modify the name of job error file for a running job.
