#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

# Shared library (libtool). This will also produce a static .a unless you
# configure with --disable-static.
lib_LTLIBRARIES = libll.la
libll_la_LDFLAGS = -version-info 0:0:0 -no-undefined
libll_la_LIBADD  = $(TIRPC_LIB) # if liblsf.a itself needs tirpc

# List your real .c files here (no Tcl/yy). Examples shown:
libll_la_SOURCES = lib.channel.c \
	lib.conf.c \
	lib.confmisc.c \
	lib.conn.c \
	lib.control.c \
	lib.cwd.c \
	lib.debug.c \
	lib.dir.c \
	lib.eauth.c \
	lib.eligible.c \
	lib.err.c \
	lib.esub.c \
	lib.hosta.c \
	lib.host.c \
	lib.info.c \
	lib.init.c \
	lib.initenv.c \
	lib.lim.c \
	lib.limits.c \
	lib.load.c \
	lib.lsf.c \
	lib.misc.c \
	lib.nioback.c \
	lib.pim.c \
	lib.place.c \
	lib.priority.c \
	lib.queue.c \
	lib.rdwr.c \
	lib.res.c \
	lib.resctrl.c \
	lib.reslog.c \
	lib.rex.c \
	lib.rwait.c \
	lib.sig.c \
	lib.sock.c \
	lib.syntax.c \
	lib.syslog.c \
	lib.table.c \
	lib.term.c \
	lib.tid.c \
	lib.tty.c \
	lib.words.c \
	lib.xdr.c \
	lib.xdrlim.c \
	lib.xdrmisc.c \
	lib.xdrnio.c \
	lib.xdrres.c \
	lib.xdrrf.c \
	lib.wconf.c \
	usleep.c

#
