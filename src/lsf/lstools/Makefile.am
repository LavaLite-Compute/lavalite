#
# Copyright (C) LavaLite Contributors
#
# lstools/Makefile.am

# Reuse shared flags and libs (AM_CPPFLAGS, TIRPC_LIB, etc.)
include $(top_srcdir)/Make.common

bin_PROGRAMS = \
    load \
    lsacct \
    lseligible \
    lshosts \
    lsid \
    lsinfo \
    lsloadadj \
    lsload \
    lsplace

# Header shared by commands (distributed, not installed)
noinst_HEADERS = lstools.h

load_SOURCES       = load.c
lsacct_SOURCES     = lsacct.c
lseligible_SOURCES = lseligible.c
lshosts_SOURCES    = lshosts.c
lsid_SOURCES       = lsid.c
lsinfo_SOURCES     = lsinfo.c
lsloadadj_SOURCES  = lsloadadj.c
lsload_SOURCES     = lsload.c

# Link all commands with libtirpc from Make.common
LDADD = ../lib/liblsf.la \
    ../intlib/liblsf_intlib.a \
    $(TIRPC_LIB)

