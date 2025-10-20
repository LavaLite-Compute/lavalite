#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

COMMON_LIBS = ../lib/libllbat.la ../../lsf/lib/libll.la \
    ../../lsf/intlib/libllcore.la -lm

LDADD = $(COMMON_LIBS)

sbin_PROGRAMS = mbatchd
mbatchd_SOURCES = mbd.main.c \
	mbd.comm.c \
	mbd.host.c \
	mbd.jgrp.c \
	mbd.resource.c \
    mbd.dep.c \
	mbd.init.c \
	mbd.job.c \
	mbd.misc.c \
	mbd.queue.c \
    mbd.serv.c \
	mbd.policy.c \
	mbd.grp.c \
	mbd.jarray.c \
	mbd.log.c \
	mbd.requeue.c \
	mbd.window.c \
    elock.c \
	misc.c \
	mail.c \
	daemons.c \
	daemons.xdr.c

mbatchd_DEPENDENCIES = $(COMMON_LIBS)

