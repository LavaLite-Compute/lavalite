import os
import pytest
from conftest import run, assert_matches_any

RUN_INTERACTIVE = os.environ.get("LAVALITE_RUN_INTERACTIVE", "0") == "1"

@pytest.mark.skipif(not RUN_INTERACTIVE, reason="set LAVALITE_RUN_INTERACTIVE=1 to enable")
def test_bsub_I_banner_and_output():
    rc, out, err = run('bsub -I /bin/echo hello', timeout=60)
    assert rc == 0, f"bsub -I failed: {err}"
    assert_matches_any(out, [
        r'Job is running on host\s+\S+',
    ], "interactive banner missing")
    assert "hello" in out, "interactive payload missing"

