#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

LIBS = ../lib/libllbat.a ../../lsf/lib/libll.a \
	../../lsf/lib/libllcore.a

bin_PROGRAMS = bhist

# Source and linker definitions
bhist_SOURCES = bhist.c read.event.c
bhist_LDADD = $(LIBS)
