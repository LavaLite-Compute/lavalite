# Copyright (C) LavaLite Contributors

AC_INIT([lavalite], [0.9.0], [https://github.com/LavaLiteProject/LavaLite/issues])
dnl Modern large/posix feature defines (instead of forcing _GNU_SOURCE)
AC_USE_SYSTEM_EXTENSIONS

AC_CONFIG_HEADERS([config.h])
AC_PREFIX_DEFAULT([/opt/lavalite-0.9.0])
AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIRS([m4])

dnl Automake / libtool
AM_INIT_AUTOMAKE([foreign -Wall subdir-objects])
AM_PROG_AR
LT_INIT([disable-shared])

dnl Compilers / tools
AC_PROG_CC
AC_PROG_INSTALL

dnl Check for OpenSSL via pkg-config
PKG_CHECK_MODULES([OPENSSL], [openssl >= 1.1.1],
  [AC_DEFINE([HAVE_OPENSSL], [1], [Define if OpenSSL is available])],
  [AC_MSG_ERROR([OpenSSL >= 1.1.1 not found])])

dnl Project identity versioning
AC_DEFINE([LAVALITE_VERSION_STR], ["LavaLite 0.9.0"], [Human-readable LavaLite version])
AC_DEFINE([LAVALITE_VERSION_HEX], [0x009000], [LavaLite version encoded as 0xMMmmpp])

dnl ---- Dependencies ----

dnl libtirpc (XDR)
PKG_CHECK_MODULES([TIRPC], [libtirpc],
  [CPPFLAGS="$CPPFLAGS $TIRPC_CFLAGS"
   LIBS="$LIBS $TIRPC_LIBS"],
  [AC_MSG_ERROR([libtirpc not found — required for XDR support])])

dnl Generate Makefiles
AC_CONFIG_FILES([
  docs/Makefile
  docs/man/Makefile
  Makefile
  src/Makefile
  src/base/Makefile
  src/base/lib/Makefile
  src/base/cmd/Makefile
  src/base/lim/Makefile
  src/batch/Makefile
  src/batch/lib/Makefile
  src/batch/cmd/Makefile
  src/batch/mbd/Makefile
  src/batch/sbd/Makefile
])

AC_OUTPUT
