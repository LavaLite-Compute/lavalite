#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

COMMON_LIBS = ../lib/libllbat.a ../../base/lib/libllbase.a -lm $(OPENSSL_LIBS)

LDADD = $(COMMON_LIBS)

bin_PROGRAMS = bhosts bjobs bkill bqueues bsub bgroups btokens bhist \
	       bmove bpriority

# Source and linker definitions
bhosts_SOURCES    = bhosts.c $(COMMON_CMD_SOURCES)
bjobs_SOURCES     = bjobs.c $(COMMON_CMD_SOURCES)
bkill_SOURCES     = bkill.c $(COMMON_CMD_SOURCES)
bgroups_SOURCES   = bgroups.c $(COMMON_CMD_SOURCES)
bqueues_SOURCES   = bqueues.c $(COMMON_CMD_SOURCES)
bsub_SOURCES      = bsub.c $(COMMON_CMD_SOURCES)
btokens_SOURCES   = btokens.c $(COMMON_CMD_SOURCES)
bhist_SOURCES   = bhist.c $(COMMON_CMD_SOURCES)
bmove_SOURCES   = bmove.c $(COMMON_CMD_SOURCES)
bpriority_SOURCES   = bpriority.c $(COMMON_CMD_SOURCES)

EXTRA_DIST = Make.common

