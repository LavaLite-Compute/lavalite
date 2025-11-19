#!/usr/bin/env python3
import json
import sys

def main():
    raw = sys.stdin.read()
    job = json.loads(raw)

    user = job["user"]
    res  = job.get("resources", {})
    queue = job.get("queue", "normal")

    # Example policy 1: cap memory per queue
    if queue == "normal":
        mem = parse_mem(res.get("mem", "0GB"))
        if mem > 64:
            res["mem"] = "64GB"

    # Example policy 2: forbid nastran outside "nastran" queue
    if res.get("license") == "nastran" and queue != "nastran":
        return reject("Nastran license jobs must use queue 'nastran'")

    # Example policy 3: force rack1 for debug queue
    if queue == "debug":
        res.setdefault("constraints", []).append("rack1")

    job["resources"] = res
    return accept(job)

def accept(job):
    json.dump({"action": "accept", "job": job}, sys.stdout)
    return 0

def reject(reason):
    json.dump({"action": "reject", "reason": reason}, sys.stdout)
    return 0

def parse_mem(s):
    # KISS: parse "100GB" -> 100, "64G" -> 64
    s = s.strip().upper()
    for suffix in ("GB", "G"):
        if s.endswith(suffix):
            return int(s[:-len(suffix)])
    return int(s)

if __name__ == "__main__":
    raise SystemExit(main())
