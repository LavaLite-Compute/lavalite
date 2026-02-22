#!/usr/bin/env python3
import argparse
import os
import random
import re
import signal
import subprocess
import sys
import time

DEFAULT_TIMEOUT = 5.0

PROC_NAMES = {
    "lim": "lim",
    "mbd": "mbd",
    "sbd": "sbd",
}

BSUB  = ["bsub"]
BHIST = ["bhist"]
BACCT = ["bacct"]


def run(cmd, timeout=DEFAULT_TIMEOUT):
    return subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
        check=False,
    )


def must_run(cmd, timeout=DEFAULT_TIMEOUT):
    cp = run(cmd, timeout=timeout)
    if cp.returncode != 0:
        sys.stderr.write(f"chaos: command failed: {' '.join(cmd)}\n")
        if cp.stdout:
            sys.stderr.write(cp.stdout)
        if cp.stderr:
            sys.stderr.write(cp.stderr)
        raise SystemExit(1)
    return cp


def extract_jobid(txt):
    m = re.search(r"Job\s+<(\d+)>", txt)
    if not m:
        return None
    return int(m.group(1))


def list_pids_by_name(name):
    cp = run(["pgrep", "-x", name], timeout=DEFAULT_TIMEOUT)
    if cp.returncode != 0:
        return []
    out = cp.stdout.strip()
    if not out:
        return []
    pids = []
    for s in out.split():
        try:
            pids.append(int(s))
        except ValueError:
            pass
    return pids


def is_up(kind):
    name = PROC_NAMES[kind]
    return len(list_pids_by_name(name)) > 0


def kill_one(kind):
    name = PROC_NAMES[kind]
    pids = list_pids_by_name(name)
    if not pids:
        return False
    pid = random.choice(pids)
    try:
        os.kill(pid, signal.SIGKILL)
    except ProcessLookupError:
        return False
    return True


def submit_one(queue, cmd):
    cp = must_run(
        BSUB + ["-q", queue,
                "-o", "/dev/null",
                "-e", "/dev/null",
                cmd], timeout=10.0)
    jid = extract_jobid(cp.stdout + "\n" + cp.stderr)
    if jid is None:
        sys.stderr.write("chaos: cannot parse jobid from bsub output\n")
        if cp.stdout:
            sys.stderr.write(cp.stdout)
        if cp.stderr:
            sys.stderr.write(cp.stderr)
        return None
    return jid


def main():
    if os.geteuid() != 0:
        raise SystemExit("chaos: must be run as root")

    envdir = os.environ.get("LSF_ENVDIR", "").strip()
    if not envdir:
        raise SystemExit("chaos: $LSF_ENVDIR is not set")

    conf = os.path.join(envdir, "lsf.conf")
    if not os.path.isfile(conf):
        raise SystemExit(f"chaos: missing {conf}")

    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--queue", default="system")
    ap.add_argument("--submit-every", type=float, default=0.5)
    ap.add_argument("--kill-every", type=float, default=1.0)
    ap.add_argument("--cooldown", type=float, default=12.0,
                    help="Minimum seconds between kills of the same daemon")
    ap.add_argument("--kinds", default="lim,mbd,sbd",
                    help="Comma list: lim,mbd,sbd")
    ap.add_argument("--sample", type=int, default=0,
                    help="Verify/print only last N jobids (0 = all)")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="Progress + submit/kill/skip lines")
    ap.add_argument("--bhist", action="store_true",
                    help="At end, run bhist and print output for verified jobs")
    args = ap.parse_args()

    # Parse kinds (KISS)
    kinds = []
    for k in args.kinds.split(","):
        k = k.strip()
        if not k:
            continue
        if k not in PROC_NAMES:
            raise SystemExit(f"chaos: invalid kind: {k}")
        kinds.append(k)

    if not kinds:
        raise SystemExit("chaos: no kinds selected")

    last_kill = {}
    for k in kinds:
        last_kill[k] = 0.0

    job_cmds = ["true", "false", "sleep 1", "sleep 2", "sleep 3", "sleep 10"]

    t_end = time.time() + args.seconds
    next_submit = 0.0
    next_kill = 0.0
    next_beat = 0.0

    jobids = []
    kills = 0
    skipped = 0

    while time.time() < t_end:
        now = time.time()

        if args.verbose and now >= next_beat:
            sys.stderr.write(
                f"chaos: left={int(t_end-now)}s submitted={len(jobids)} "
                f"killed={kills} skipped={skipped}\n"
            )
            next_beat = now + 5.0

        if now >= next_submit:
            cmd = random.choice(job_cmds)
            jid = submit_one(args.queue, cmd)
            if jid is not None:
                jobids.append(jid)
                if args.verbose:
                    sys.stderr.write(f"chaos: submit job <{jid}> cmd={cmd}\n")
            next_submit = now + args.submit_every

        if now >= next_kill:
            victim = random.choice(kinds)

            can_kill = True
            if (now - last_kill[victim]) < args.cooldown:
                can_kill = False
            elif not is_up(victim):
                can_kill = False

            if can_kill:
                if kill_one(victim):
                    kills += 1
                    last_kill[victim] = now
                    if args.verbose:
                        sys.stderr.write(f"chaos: kill {victim}\n")
                else:
                    skipped += 1
                    if args.verbose:
                        sys.stderr.write(f"chaos: skip kill {victim}\n")
            else:
                skipped += 1
                if args.verbose:
                    sys.stderr.write(f"chaos: skip kill {victim}\n")

            next_kill = now + args.kill_every

        time.sleep(0.05)

    # Let the supervisor restart and the system converge
    time.sleep(5.0)

    # Decide which jobs to verify/print
    jobs_to_check = jobids
    if args.sample and args.sample > 0:
        jobs_to_check = jobids[-args.sample:]

    bad = 0

    for jid in jobs_to_check:
        cp = run(BHIST + [str(jid)], timeout=10.0)
        if cp.returncode != 0:
            bad += 1
            sys.stderr.write(f"chaos: bhist failed for job {jid}\n")
            if cp.stderr:
                sys.stderr.write(cp.stderr)

        if args.bhist:
            sys.stderr.write(f"\n===== bhist <{jid}> =====\n")
            if cp.stdout:
                sys.stderr.write(cp.stdout)
            if cp.stderr:
                sys.stderr.write(cp.stderr)

        # bacct may legitimately not have it yet; do not hard-fail.
        run(BACCT + [str(jid)], timeout=10.0)

    print(
        f"chaos: done (submitted {len(jobids)} jobs, killed {kills}, "
        f"skipped {skipped}, bhist_fail={bad})"
    )

    if bad != 0:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
