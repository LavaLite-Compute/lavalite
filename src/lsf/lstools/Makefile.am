#
# Copyright (C) LavaLite Contributors
#
# lstools/Makefile.am

# Reuse shared flags and libs (AM_CPPFLAGS, TIRPC_LIB, etc.)
include $(top_srcdir)/Make.common
COMMON_LIBS = ../lib/liblavalite.la ../intlib/libllcore.la $(TIRPC_LIB)

bin_PROGRAMS = \
    lsacct \
    lseligible \
    lshosts \
    lsid \
    lsinfo \
    lsloadadj \
    lsload \
    lsplace

# Command source and linker
lsacct_SOURCES     = lsacct.c
lsacct_LDADD = $(COMMON_LIBS)
lseligible_SOURCES = lseligible.c
lseligible_LDADD = $(COMMON_LIBS)
lshosts_SOURCES    = lshosts.c
lshosts_LDADD = $(COMMON_LIBS)
lsid_SOURCES       = lsid.c
lsid_LDADD = $(COMMON_LIBS)
lsinfo_SOURCES     = lsinfo.c
lsinfo_LDADD = $(COMMON_LIBS)
lsloadadj_SOURCES  = lsloadadj.c
lsloadadj_LDADD = $(COMMON_LIBS)
lsload_SOURCES     = lsload.c load.c
lsload_LDADD = $(COMMON_LIBS)
lsplace_SOURCES = lsplace.c
lsplace_LDADD = $(COMMON_LIBS)
