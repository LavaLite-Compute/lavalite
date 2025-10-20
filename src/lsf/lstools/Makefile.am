#
# Copyright (C) LavaLite Contributors
#
# lstools/Makefile.am

include $(top_srcdir)/Make.common
COMMON_LIBS = ../lib/libll.la ../intlib/libllcore.la

LDADD = $(COMMON_LIBS)

bin_PROGRAMS = lsacct lseligible lshosts lsid lsinfo \
	lsloadadj lsload lsplace

# Command source and linker
lsacct_SOURCES     = lsacct.c
lsacct_DEPENDENCIES = $(COMMON_LIBS)
lseligible_SOURCES = lseligible.c
lshosts_SOURCES    = lshosts.c
lsid_SOURCES       = lsid.c
lsinfo_SOURCES     = lsinfo.c
lsloadadj_SOURCES  = lsloadadj.c
lsload_SOURCES     = lsload.c load.c
lsplace_SOURCES = lsplace.c

