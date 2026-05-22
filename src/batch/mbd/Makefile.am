#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

COMMON_LIBS = ../lib/libllbat.a ../../base/lib/libllbase.a -lm $(OPENSSL_LIBS)

LDADD = $(COMMON_LIBS)

sbin_PROGRAMS = mbd
mbd_SOURCES = mbd.c conf.c  sched.c events.c net.c dispatch.c job.c \
	      sbd.c admin.c
# mbd_SOURCES = main.c api.c compact.c events.c init.c job.c net.c \
#	      sbd.c sched.c

mbd_DEPENDENCIES = $(COMMON_LIBS)

