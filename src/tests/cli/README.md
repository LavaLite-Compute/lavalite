# CLI Compatibility Tests (Automake + pytest)

This suite checks that LavaLite’s CLI matches LSF-style behavior for:
- `lsid`, `lsload`, `lshosts`, `bhosts`
- `bsub` (submit) and `bjobs` (basic polling)
- `bsub -I` interactive banner (opt-in)

## Prerequisites
- Python 3
- `pytest` in PATH (`pip install pytest`)

`configure` will autodetect pytest; if not found, tests are skipped.

## Build & Run

From the project root:
```sh
autoreconf -fi
./configure
make
make check             # runs non-interactive smoke tests

