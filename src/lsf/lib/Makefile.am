#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

# Shared library (libtool). This will also produce a static .a unless you
# configure with --disable-static.
lib_LIBRARIES = libll.a
#libll_la_LDFLAGS = -version-info 0:0:0 -no-undefined
#libll_a_LIBADD  = $(TIRPC_LIB) # if liblsf.a itself needs tirpc
libll_a_LIBADD  = $(TIRPC_LIB) # if liblsf.a itself needs tirpc

# List your real .c files here (no Tcl/yy). Examples shown:
# libll_la_SOURCES
libll_a_SOURCES = \
    admin.c \
    bitset.c \
    cmdtime.c \
    daemon.c \
    list.c \
    listset.c \
    misc.c \
    resreq.c \
    testbitset.c \
    userok.c \
    window.c \
    resourcecmd.c \
    lib.channel.c \
    lib.conf.c \
    lib.confmisc.c \
    lib.control.c \
    lib.cwd.c \
    lib.debug.c \
    lib.eauth.c \
    lib.err.c \
    lib.esub.c \
    lib.info.c \
    lib.initenv.c \
    lib.lim.c \
    lib.limits.c \
    lib.lsf.c \
    lib.misc.c \
    lib.place.c \
    lib.priority.c \
    lib.rdwr.c \
    lib.sig.c \
    lib.syslog.c \
    lib.table.c \
    lib.words.c \
    lib.xdr.c \
    lib.xdrlim.c \
    lib.wconf.c \
    ll.conf.c \
    ll.host.c \
    ll.timer.c \
    tcl_stub.c

