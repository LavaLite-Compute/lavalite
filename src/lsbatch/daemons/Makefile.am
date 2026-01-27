#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

COMMON_LIBS = ../lib/libllbat.a ../../lsf/lib/libll.a -lm

LDADD = $(COMMON_LIBS)

sbin_PROGRAMS = mbatchd sbatchd
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
	daemons.xdr.c \
    mbatchd.handler.c \
    mbatchd.job.c

sbatchd_SOURCES =  sbatchd.main.c sbatchd.handler.c sbatchd.net.c \
                   sbatchd.job.c sbatchd.client.c \
                   daemons.xdr.c daemons.c sbatchd.log.c

mbatchd_DEPENDENCIES = $(COMMON_LIBS)
sbatchd_DEPENDENCIES = $(COMMON_LIBS)

