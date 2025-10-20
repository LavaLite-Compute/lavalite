#
# Copyright (C) LavaLite Contributors
#

# Reuse shared flags and libs (AM_CPPFLAGS, TIRPC_LIB, etc.)
include $(top_srcdir)/Make.common
AM_CPPFLAGS += $(NO_SIGALARM)

COMMON_LIBS = ../lib/libll.la ../intlib/libllcore.la $(TIRPC_LIB)

sbin_PROGRAMS = res

res_SOURCES = res.c \
	res.getproc.c \
	res.handler.c \
	res.init.c \
	res.misc.c \
	res.pty.c

res_LDADD = $(COMMON_LIBS)
