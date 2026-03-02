#!/usr/bin/env python3
import argparse
import datetime
import os
import random
import re
import socket
import subprocess
import sys
import time

# Chaos tool contract:
# - caller username exists uniformly on all hosts
# - ssh is non-interactive (keys)
# - sudo -n works on all target hosts
# - systemd is configured for aggressive restarts (e.g. StartLimitIntervalSec=0)

DEFAULT_TIMEOUT = 5.0
BSUB_TIMEOUT = 15.0
SSH_CONNECT_TIMEOUT = 3

PROC_NAMES = {
    "lim": "lim",
    "mbd": "mbd",
    "sbd": "sbd",
}

# Single source of truth:
# - kinds controls what can be killed
# - remote-capable policy is fixed (avoid a second list that can drift)
REMOTE_CAPABLE = {"lim", "sbd"}   # mbd is local-only by default

BSUB = ["bsub"]
LOCAL_HOST = socket.gethostname()


def ts_now():
    now = datetime.datetime.now()
    return now.strftime("%b %d %H:%M:%S.") + f"{int(now.microsecond / 1000):03d}"


def log(msg):
    sys.stderr.write(f"{ts_now()} {msg}\n")


def run(cmd, timeout=DEFAULT_TIMEOUT):
    try:
        return subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as e:
        out = e.stdout if e.stdout is not None else ""
        err = e.stderr if e.stderr is not None else ""
        if not err:
            err = f"timeout after {timeout:.1f}s"
        return subprocess.CompletedProcess(cmd, 124, out, err)


def ssh_prefix(host):
    if host == LOCAL_HOST:
        return []

    return [
        "ssh",
        "-o", "BatchMode=yes",
        "-o", "ConnectionAttempts=1",
        "-o", f"ConnectTimeout={SSH_CONNECT_TIMEOUT}",
        "-o", "StrictHostKeyChecking=accept-new",
        host,
    ]


def run_on_host(host, cmd, timeout=DEFAULT_TIMEOUT):
    return run(ssh_prefix(host) + cmd, timeout=timeout)


def read_hosts_file(path):
    # Format: one host per line. Comments allowed.
    hosts = []

    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            hosts.append(line.split()[0])

    return hosts


def parse_kinds(s):
    kinds = []
    for x in s.split(","):
        x = x.strip()
        if not x:
            continue
        if x not in PROC_NAMES:
            raise SystemExit(f"chaos: invalid kind: {x}")
        kinds.append(x)

    if not kinds:
        raise SystemExit("chaos: no kinds selected")

    return kinds


def extract_jobid(txt):
    m = re.search(r"Job\s+<(\d+)>", txt)
    if not m:
        return None
    return int(m.group(1))


def submit_one(queue, cmd):
    cp = run(
        BSUB + ["-q", queue, "-o", "/dev/null", "-e", "/dev/null", cmd],
        timeout=BSUB_TIMEOUT,
    )
    if cp.returncode != 0:
        return None
    return extract_jobid(cp.stdout + "\n" + cp.stderr)


def list_pids(host, name):
    cp = run_on_host(host, ["pgrep", "-x", name], timeout=DEFAULT_TIMEOUT)
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


def is_up(host, kind):
    return len(list_pids(host, PROC_NAMES[kind])) > 0


def kill_one(host, kind):
    pids = list_pids(host, PROC_NAMES[kind])
    if not pids:
        return False

    pid = random.choice(pids)
    cp = run_on_host(host, ["sudo", "-n", "kill", "-9", str(pid)], timeout=DEFAULT_TIMEOUT)
    return cp.returncode == 0


def main():
    envdir = os.environ.get("LSF_ENVDIR", "").strip()
    if not envdir:
        raise SystemExit("chaos: $LSF_ENVDIR is not set")

    conf = os.path.join(envdir, "lsf.conf")
    if not os.path.isfile(conf):
        raise SystemExit(f"chaos: missing {conf}")

    ap = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        description="Chaos test: submit jobs and randomly kill daemons across hosts.",
    )
    ap.add_argument("--hosts-file", required=True,
                    help="Host list file (one host per line)")
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--queue", default="system")
    ap.add_argument("--submit-every", type=float, default=1.0)
    ap.add_argument("--kill-every", type=float, default=1.0)
    ap.add_argument("--kinds", default="lim,mbd,sbd",
                    help="Comma list: lim,mbd,sbd")
    args = ap.parse_args()

    hosts = read_hosts_file(args.hosts_file)
    if not hosts:
        raise SystemExit("chaos: empty host list")

    kinds = parse_kinds(args.kinds)

    job_cmds = ["true", "false", "sleep 1", "sleep 2", "sleep 3", "sleep 10"]

    t_end = time.time() + args.seconds
    next_submit = time.time()
    next_kill = time.time()

    submits_ok = 0
    submits_fail = 0
    kills_ok = 0
    kills_skip = 0
    jobids = []

    last_killed = None  # (kind, host) to avoid pathological immediate repeats

    while time.time() < t_end:
        now = time.time()

        if now >= next_submit:
            cmd = random.choice(job_cmds)
            jid = submit_one(args.queue, cmd)

            if jid is not None:
                submits_ok += 1
                jobids.append(jid)
                log(f"chaos: submit job <{jid}> cmd={cmd}")
            else:
                submits_fail += 1
                log(f"chaos: submit failed cmd={cmd}")

            next_submit = now + args.submit_every

        if now >= next_kill:
            victim_kind = random.choice(kinds)

            if victim_kind in REMOTE_CAPABLE:
                victim_host = random.choice(hosts) if hosts else LOCAL_HOST
            else:
                victim_host = LOCAL_HOST

            pair = (victim_kind, victim_host)
            if last_killed == pair and random.random() < 0.7:
                # Avoid hammering the exact same pair repeatedly.
                next_kill = now + (args.kill_every * random.uniform(0.3, 0.8))
            else:
                if not is_up(victim_host, victim_kind):
                    kills_skip += 1
                    log(f"chaos: skip kill {victim_kind} host={victim_host} (down)")
                else:
                    if kill_one(victim_host, victim_kind):
                        kills_ok += 1
                        last_killed = pair
                        log(f"chaos: kill {victim_kind} host={victim_host}")
                    else:
                        kills_skip += 1
                        log(f"chaos: skip kill {victim_kind} host={victim_host} (fail)")

                # Jitter makes the test less “metronomic” without pretending
                # we know systemd restart policy.
                next_kill = now + (args.kill_every * random.uniform(0.5, 1.5))

        time.sleep(0.05)

    time.sleep(5.0)

    last_jobid = jobids[-1] if jobids else 0
    print(
        f"chaos: duration={args.seconds}s ok={submits_ok} fail={submits_fail} "
        f"killed={kills_ok} skipped={kills_skip} last_jobid={last_jobid}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
