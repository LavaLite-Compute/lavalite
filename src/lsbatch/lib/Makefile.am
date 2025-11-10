#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common
AM_CFLAGS   = -Wall -Wextra -g -O0

lib_LIBRARIES = libllbat.a
#libllbat_la_LDFLAGS = -version-info 0:0:0 -no-undefined
#libllbat_la_LIBADD  = $(TIRPC_LIB)

# Create a symlink for legacy compatibility
install-exec-hook:
	@echo "Creating compatibility symlink liblsbatch.la -> libllbat.so"
	cd $(DESTDIR)$(libdir) && ln -sf libllbat.a liblsbatch.a


libllbat_a_SOURCES = lsb.comm.c \
	lsb.conf.c \
	lsb.debug.c \
	lsb.err.c \
	lsb.groups.c \
	lsb.hc.c \
	lsb.hosts.c \
	lsb.init.c \
	lsb.jobs.c \
	lsb.log.c \
	lsb.mig.c \
	lsb.misc.c \
	lsb.modify.c \
	lsb.move.c \
	lsb.msg.c \
	lsb.params.c \
	lsb.peek.c \
	lsb.qc.c \
	lsb.queues.c \
	lsb.reason.c \
	lsb.reconfig.c \
	lsb.resource.c \
	lsb.sig.c \
	lsb.signals.c \
	lsb.sub.c \
	lsb.switch.c \
	lsb.users.c \
	lsb.xdr.c
