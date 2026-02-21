#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

COMMON_LIBS = ../lib/libllbat.a ../../lsf/lib/libll.a

LDADD = $(COMMON_LIBS)

bin_PROGRAMS = bhosts \
    bjobs \
    bkill \
    bmgroup \
    bmod \
    bparams \
    bpeek \
    bqueues \
    bsub \
    bswitch \
    busers
bin_SCRIPTS = bacct bhist
# Source and linker definitions
COMMON_CMD_SOURCES = cmd.misc.c cmd.move.c cmd.jobid.c cmd.prt.c \
	cmd.bhc.c cmd.err.c cmd.job.c cmd.sub.c \
	cmd.bqc.c  cmd.hist.c
bhosts_SOURCES    = bhosts.c $(COMMON_CMD_SOURCES)
bjobs_SOURCES     = bjobs.c $(COMMON_CMD_SOURCES)
bkill_SOURCES     = bkill.c $(COMMON_CMD_SOURCES)
bmgroup_SOURCES   = bmgroup.c $(COMMON_CMD_SOURCES)
bmod_SOURCES      = bmod.c $(COMMON_CMD_SOURCES)
bparams_SOURCES   = bparams.c $(COMMON_CMD_SOURCES)
bpeek_SOURCES     = bpeek.c $(COMMON_CMD_SOURCES)
bqueues_SOURCES   = bqueues.c $(COMMON_CMD_SOURCES)
bsub_SOURCES      = bsub.c $(COMMON_CMD_SOURCES)
bswitch_SOURCES   = bswitch.c $(COMMON_CMD_SOURCES)
busers_SOURCES    = busers.c $(COMMON_CMD_SOURCES)

EXTRA_DIST = Make.common

