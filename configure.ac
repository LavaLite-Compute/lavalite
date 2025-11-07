# LavaLite HPC Scheduler - Autoconf Setup
# Copyright (C) LavaLite Contributors

AC_INIT([LavaLite], [0.1.0], [https://github.com/LavaLiteProject/LavaLite/issues])
AC_CONFIG_HEADERS([config.h])
AC_PREFIX_DEFAULT([/opt/lavalite-0.1.0])
AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIRS([m4])

dnl Modern large/posix feature defines (instead of forcing _GNU_SOURCE)
AC_USE_SYSTEM_EXTENSIONS

dnl Check for pkg-config
AC_PATH_PROG([PKG_CONFIG], [pkg-config], [no])
if test "$PKG_CONFIG" = "no"; then
  AC_MSG_ERROR([pkg-config is required but not found])
fi

dnl Check for OpenSSL via pkg-config
PKG_CHECK_MODULES([OPENSSL], [openssl >= 1.1.1],
  [AC_DEFINE([HAVE_OPENSSL], [1], [Define if OpenSSL is available])],
  [AC_MSG_ERROR([OpenSSL >= 1.1.1 not found])])

dnl Project identity versioning
AC_DEFINE([LAVALITE_VERSION_STR], ["LavaLite 0.1.0"], [Human-readable LavaLite version])
AC_DEFINE([LAVALITE_VERSION_HEX], [0x000100], [LavaLite version encoded as 0xMMmmpp])

dnl Define the default host type supported by LavaLite but you can
dnl add more in lsf.shared file.
AC_DEFINE([HOST_TYPE_STRING], ["x86_64"], [Define host type string])

dnl Resolve Autoconf's lazy dir vars so SYSCONFDIR is a concrete path in config.h.
dnl - If --prefix is not given, $prefix is "NONE" → use $ac_default_prefix.
dnl - $sysconfdir usually contains ${prefix}/etc → expand it now via eval.
dnl Without this, config.h would contain literally "${prefix}/etc".
if test "x$prefix" = xNONE; then
  prefix="$ac_default_prefix"     # e.g., /opt/lavalite-0.1.0 from AC_PREFIX_DEFAULT
fi
if test "x$exec_prefix" = xNONE; then
  exec_prefix='${prefix}'         # keep the normal relationship
fi
eval sysconfdir="$sysconfdir"     # expand nested ${...} → real path
AC_DEFINE_UNQUOTED([SYSCONFDIR], ["$sysconfdir"], [System configuration directory])

dnl Automake / libtool
AM_INIT_AUTOMAKE([foreign -Wall subdir-objects])
AM_PROG_AR
dnl AM_SILENT_RULES([yes])
LT_INIT

dnl Compilers / tools
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_YACC
dnl AC_PROG_LEX

dnl CFLAGS (GNU)
if test "x$ac_compiler_gnu" = xyes; then
  CFLAGS="$CFLAGS -Wall -fPIC"
fi

dnl No berkley email...
AC_DEFINE([NO_MAIL], [1], [Define to disable Berkeley email routing])

dnl -------- CLI tests tooling --------

AC_PATH_PROG([PYTEST], [pytest])
if test -z "$PYTEST"; then
  AC_MSG_WARN([pytest not found; CLI tests will be skipped. Install pytest to enable.])
fi

AC_ARG_ENABLE([cli-tests],
  AS_HELP_STRING([--enable-cli-tests], [Enable Python CLI tests (default: yes if pytest found)]),
  [enable_cli_tests=$enableval],
  [enable_cli_tests=yes])

AM_CONDITIONAL([HAVE_PYTEST], [test "x$PYTEST" != "x" -a "x$enable_cli_tests" = "xyes"])

dnl Generate Makefile for tests
AC_CONFIG_FILES([
	src/tests/Makefile
	src/tests/cli/Makefile
])

dnl ---- Dependencies ----

dnl Check for ncurses library
AC_CHECK_LIB([ncurses], [initscr], [
  LIBS="$LIBS -lncurses"
], [
  AC_MSG_ERROR([Cannot build LavaLite without libncurses])
])

dnl libtirpc (XDR)
CPPFLAGS="$CPPFLAGS -I/usr/include/tirpc"
AC_CHECK_HEADERS([rpc/xdr.h], [], [
  AC_MSG_ERROR([Missing rpc/xdr.h — required for XDR support])
])
AC_SEARCH_LIBS([xdrmem_create], [tirpc], [], [
  AC_MSG_ERROR([Missing libtirpc (-ltirpc)])
])

dnl libnsl (NIS)
AC_CHECK_LIB([nsl], [yp_get_default_domain], [
  LIBS="$LIBS -lnsl"
], [
  AC_MSG_ERROR([Missing libnsl — required for NIS support])
])

dnl Check for pdflatex
AC_PATH_PROG([PDFLATEX], [pdflatex])
AM_CONDITIONAL([HAVE_PDFLATEX], [test -n "$PDFLATEX"])
dnl Generate Makefiles for doc
AC_CONFIG_FILES([
  docs/Makefile
  docs/man/Makefile
  docs/projects/Makefile
])

dnl ---- Top-level ----
AC_CONFIG_FILES([
  Makefile
  src/Makefile
])

dnl ---- External Authentication ----
AC_CONFIG_FILES([
  src/eauth/Makefile
])

dnl ---- LSF module ----
AC_CONFIG_FILES([
  src/lsf/Makefile
  src/lsf/lib/Makefile
  src/lsf/intlib/Makefile
  src/lsf/lstools/Makefile
  src/lsf/lim/Makefile
  src/lsf/pim/Makefile
])

dnl ---- LSBATCH module ----
AC_CONFIG_FILES([
  src/lsbatch/Makefile
  src/lsbatch/lib/Makefile
  src/lsbatch/cmd/Makefile
  src/lsbatch/daemons/Makefile
])

AC_OUTPUT
