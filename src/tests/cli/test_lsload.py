from conftest import run

def test_lsload_headers():
    rc, out, err = run("lsload")
    assert rc == 0, f"lsload failed: {err}"
    hdr = out.splitlines()[0] if out else ""
    for token in ["HOST_NAME", "status", "r15s", "r1m", "r15m"]:
        assert token in hdr, f"missing column {token} in: {hdr}"

