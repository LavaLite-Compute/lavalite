#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

COMMON_LIBS = ../lib/libllbat.a ../../base/lib/libllbase.a -lm $(OPENSSL_LIBS)

LDADD = $(COMMON_LIBS)

sbin_PROGRAMS = spd
spd_SOURCES = spmain.c spnet.c

spd_DEPENDENCIES = $(COMMON_LIBS)

