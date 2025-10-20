#
# Copyright (C) LavaLite Contributors
#

include $(top_srcdir)/Make.common

AM_CPPFLAGS += -I$(top_srcdir)/src/lsf/lim \
  -I$(top_srcdir)/src/lsf/res

noinst_LTLIBRARIES = libllcore.la
libllcore_la_SOURCES = admin.c bitset.c cmdtime.c conf.c \
	daemon.c list.c misc.c resourcecmd.c resreq.c \
	userok.c wait.c window.c tcl_stub.c listset.c

# Data headers or tables shipped in the tarball
EXTRA_DIST = listerr.def lsbitseterr.def
