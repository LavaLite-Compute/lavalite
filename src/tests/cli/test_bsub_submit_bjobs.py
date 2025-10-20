import re
from conftest import run, poll_until

_jobid_re = re.compile(r'Job\s+<(\d+)>.*submitted.*queue\s+<([^>]+)>', re.I)

def submit_true():
    rc, out, err = run("bsub /bin/true", timeout=10)
    # Some implementations return 255 on submit error paths; accept both.
    assert rc in (0, 255), f"bsub failed unexpectedly: {rc} {err or out}"
    m = _jobid_re.search(out)
    assert m, f"unexpected submit line: {out}"
    jobid = int(m.group(1))
    queue = m.group(2)
    return jobid, queue, out

def _bjobs_state(jobid):
    rc, out, err = run(f"bjobs -noheader {jobid}")
    if rc != 0:
        return False, (rc, out, err)
    # Accept common states
    ok = re.search(rf'^{jobid}\b.*\b(PEND|RUN|DONE|EXIT|FINISH|ZOMBI)\b', out, re.M) is not None
    return ok, out

def test_bsub_submit_and_bjobs_poll():
    jobid, queue, submit_out = submit_true()
    assert queue, "queue should be non-empty"
    out = poll_until(lambda: _bjobs_state(jobid), seconds=20, interval=0.5)
    assert out, f"bjobs never showed job {jobid}\nsubmit:\n{submit_out}"

