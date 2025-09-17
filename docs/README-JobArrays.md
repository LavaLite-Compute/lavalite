LSF job arrays let you submit many similar jobs with a single command. Use bsub
-J "name[1-100]" ... to create sub-jobs indexed from 1 to 100; each sub-job
receives its index in the environment as LSB_JOBINDEX, which you can use inside
your script to select inputs and write per-index outputs (e.g., run.sh
$LSB_JOBINDEX). You can also cap how many array elements run at once
with a concurrency limit: bsub -J "name[1-100]%10" ... runs at most 10 sub-jobs
simultaneously, queueing the rest automatically.

Index lists support both ranges and comma-separated values, e.g., -J
"name[1-5,8,12-15]". Arrays share one master job ID; individual elements appear
as JOBID[index] in bjobs. Management commands can target the whole array or a
single element—bkill JOBID cancels everything, while bkill JOBID[17] cancels
just index 17—making arrays an efficient way to scale parameter sweeps and batch
workloads with minimal submission overhead.
