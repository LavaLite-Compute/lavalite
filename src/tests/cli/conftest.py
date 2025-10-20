import os, re, subprocess, time

BIN_PREFIX = os.environ.get("LAVALITE_BIN", "")
TIMEOUT = int(os.environ.get("LAVALITE_TEST_TIMEOUT", "20"))

def _patch_cmd(cmd):
    # If BIN_PREFIX is set, rewrite the first bare word to use that prefix.
    if not BIN_PREFIX:
        return cmd
    parts = cmd.strip().split()
    if not parts:
        return cmd
    if "/" not in parts[0]:
        parts[0] = os.path.join(BIN_PREFIX, parts[0])
    return " ".join(parts)

def run(cmd, timeout=TIMEOUT, input_bytes=None, env=None):
    cmd2 = _patch_cmd(cmd)
    p = subprocess.Popen(cmd2, shell=True, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    try:
        out, err = p.communicate(input=input_bytes, timeout=timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        out, err = p.communicate()
        raise AssertionError(f"timeout: {cmd2}\nstdout:\n{out.decode()}\nstderr:\n{err.decode()}")
    return p.returncode, out.decode(errors="replace"), err.decode(errors="replace")

def assert_matches_any(text, patterns, msg=""):
    for pat in patterns:
        if re.search(pat, text, re.M):
            return
    raise AssertionError((msg or "no pattern matched")
                         + f"\nTEXT:\n{text}\nPATTERNS:\n" + "\n".join(patterns))

def poll_until(fn, seconds=30, interval=0.5):
    deadline = time.time() + seconds
    last = None
    while time.time() < deadline:
        ok, last = fn()
        if ok:
            return last
        time.sleep(interval)
    return last

