#!/usr/bin/env python3
import argparse
import os
import random
import re
import subprocess
import sys
import time

DEFAULT_TIMEOUT = 5.0

PROC_NAMES = {
    "lim": "lim",
    "mbd": "mbd",
    "sbd": "sbd",
}

BSUB = ["bsub"]


def run(cmd, timeout=DEFAULT_TIMEOUT):
    return subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
        check=False,
    )


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


def sudo_kill(pid):
    cp = run(["sudo", "-n", "kill", "-9", str(pid)], timeout=DEFAULT_TIMEOUT)
    return cp.returncode == 0


def kill_one(kind):
    name = PROC_NAMES[kind]
    pids = list_pids_by_name(name)
    if not pids:
        return False
    pid = random.choice(pids)
    return sudo_kill(pid)


def submit_one(queue, cmd):
    cp = run(
        BSUB + ["-q", queue, "-o", "/dev/null", "-e", "/dev/null", cmd],
        timeout=10.0,
    )
    if cp.returncode != 0:
        return None
    return extract_jobid(cp.stdout + "\n" + cp.stderr)


def log(msg, quiet):
    if not quiet:
        sys.stderr.write(msg + "\n")


def main():
    start_ts = time.time()

    envdir = os.environ.get("LSF_ENVDIR", "").strip()
    if not envdir:
        raise SystemExit("chaos: $LSF_ENVDIR is not set")

    conf = os.path.join(envdir, "lsf.conf")
    if not os.path.isfile(conf):
        raise SystemExit(f"chaos: missing {conf}")

    ap = argparse.ArgumentParser(
        description=(
            "Run a chaos experiment for a fixed duration while submitting jobs "
            "and randomly restarting daemons.\n"
            "Expected submitted jobs \u2248 seconds / submit-every."
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument("--seconds", type=int, default=60,
                    help="Duration of the experiment in seconds")
    ap.add_argument("--queue", default="system")
    ap.add_argument("--submit-every", type=float, default=0.5,
                    help="Submit one job every N seconds during the experiment")
    ap.add_argument("--kill-every", type=float, default=1.0,
                    help="Attempt one daemon kill every N seconds during the experiment")
    ap.add_argument("--cooldown", type=float, default=12.0,
                    help="Minimum seconds between kills of the same daemon")
    ap.add_argument("--kinds", default="lim,mbd,sbd",
                    help="Comma list: lim,mbd,sbd")
    ap.add_argument("-q", "--quiet", action="store_true",
                    help="Disable progress output (only print final summary)")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="Extra periodic progress line (in addition to events)")
    args = ap.parse_args()

    # Parse kinds
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

    last_kill = {k: 0.0 for k in kinds}

    job_cmds = ["true", "false", "sleep 1", "sleep 2", "sleep 3", "sleep 10"]

    t_end = time.time() + args.seconds
    next_submit = 0.0
    next_kill = 0.0
    next_beat = 0.0

    jobids = []
    kills = 0
    skipped = 0
    submits_ok = 0
    submits_fail = 0

    while time.time() < t_end:
        now = time.time()

        if args.verbose and not args.quiet and now >= next_beat:
            left = int(t_end - now)
            sys.stderr.write(
                f"chaos: left={left}s ok={submits_ok} fail={submits_fail} "
                f"killed={kills} skipped={skipped}\n"
            )
            next_beat = now + 5.0

        if now >= next_submit:
            cmd = random.choice(job_cmds)
            jid = submit_one(args.queue, cmd)
            if jid is not None:
                jobids.append(jid)
                submits_ok += 1
                log(f"chaos: submit job <{jid}> cmd={cmd}", args.quiet)
            else:
                submits_fail += 1
                log(f"chaos: submit failed cmd={cmd}", args.quiet)
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
                    log(f"chaos: kill {victim}", args.quiet)
                else:
                    skipped += 1
                    log(f"chaos: skip kill {victim}", args.quiet)
            else:
                skipped += 1
                log(f"chaos: skip kill {victim}", args.quiet)

            next_kill = now + args.kill_every

        time.sleep(0.05)

    # Let systemd restart and the system converge
    time.sleep(5.0)

    elapsed = time.time() - start_ts
    attempts = submits_ok + submits_fail
    last_jobid = jobids[-1] if jobids else 0

    print(
        f"chaos: duration={args.seconds}s submit_every={args.submit_every}s "
        f"attempts={attempts} ok={submits_ok} fail={submits_fail} "
        f"kills={kills} skipped={skipped} last_jobid={last_jobid} "
        f"wallclock={elapsed:.3f}s"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
