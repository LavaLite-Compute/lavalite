from conftest import run

def _assert_header_has(out, cols):
    hdr = out.splitlines()[0] if out else ""
    for c in cols:
        assert c in hdr, f"missing {c} in header: {hdr}"

def test_lshosts_headers():
    rc, out, err = run("lshosts")
    assert rc == 0, f"lshosts failed: {err}"
    _assert_header_has(out, ["HOST_NAME", "model", "status"])

def test_bhosts_headers():
    rc, out, err = run("bhosts")
    assert rc == 0, f"bhosts failed: {err}"
    _assert_header_has(out, ["HOST_NAME", "status"])

