# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

COMMON_LIBS = ../../batch/lib/libllbat.a ../../base/lib/libllbase.a \
	      -lm $(OPENSSL_LIBS)

LDADD = $(COMMON_LIBS)

noinst_PROGRAMS = netns

netns_SOURCES = netns.c ../../batch/sbd/snamespace.c

netns_DEPENDENCIES = $(COMMON_LIBS)
