# KISS Automake test harness for CLI tests

# just assume pytest is installed; if not, make check will fail clearly
PYTEST = pytest

# simple environment
AM_TESTS_ENVIRONMENT = \
  export LAVALITE_BIN="$(abs_top_builddir)/bin"; \
  export LAVALITE_TEST_TIMEOUT=25;

# list of python tests (space or backslash-separated, no blank line at end)
TESTS = \
  test_lsid.py \
  test_lsload.py \
  test_lshosts_bhosts.py \
  test_bsub_submit_bjobs.py

# extra files that should ship in dist tarball
EXTRA_DIST = \
  conftest.py \
  pytest.ini \
  README.md \
  $(TESTS) \
  test_bsub_interactive.py

# let Automake know these are pytest-based tests
TEST_EXTENSIONS = .py
PY_LOG_COMPILER = $(PYTEST)

# optional target for interactive test
.PHONY: check-interactive
check-interactive:
	LAVALITE_RUN_INTERACTIVE=1 LAVALITE_TEST_TIMEOUT=60 $(PYTEST) -q test_bsub_interactive.py

