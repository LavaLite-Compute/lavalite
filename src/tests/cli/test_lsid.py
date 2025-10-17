from conftest import run, assert_matches_any

def test_lsid_header():
    rc, out, err = run("lsid")
    assert rc == 0, f"lsid failed: {err}"
    assert out.strip(), "lsid produced no output"
    assert_matches_any(out, [
        r'\bPlatform LSF\b',   # legacy
        r'\bLavaLite\b',       # your branding
        r'\bOpenLava\b',       # transitional
    ], "lsid header mismatch")

