#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

COMMON_LIBS = ../lib/libllbat.a ../../base/lib/libllbase.a -lm

LDADD = $(COMMON_LIBS)

sbin_PROGRAMS = sbd
sbd_SOURCES = main.c

sbd_DEPENDENCIES = $(COMMON_LIBS)

