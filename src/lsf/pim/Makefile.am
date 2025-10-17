#
# Copyright (C) LavaLite Contributors
#

# Reuse shared flags and libs (AM_CPPFLAGS, TIRPC_LIB, etc.)
include $(top_srcdir)/Make.common

COMMON_LIBS = ../lib/libll.la ../intlib/libllcore.la $(TIRPC_LIB)

sbin_PROGRAMS = pim

pim_SOURCES = pim.main.c pim.linux.x

pim_LDADD = $(COMMON_LIBS) pim.linux.o
