#
# Copyright (C) LavaLite Contributors
#

# Reuse shared flags and libs (AM_CPPFLAGS, TIRPC_LIB, etc.)
include $(top_srcdir)/Make.common
AM_CPPFLAGS += $(NO_SIGALARM)

COMMON_LIBS = ../lib/libll.la ../intlib/libllcore.la $(TIRPC_LIB)

sbin_PROGRAMS = lim

lim_SOURCES = lim.cluster.c \
	lim.conf.c \
	lim.control.c \
	lim.info.c \
	lim.internal.c \
	lim.load.c \
	lim.main.c \
	lim.misc.c \
	lim.policy.c \
	lim.rload.c \
	lim.xdr.c \
    lim.linux.c

lim_LDADD = $(COMMON_LIBS)
