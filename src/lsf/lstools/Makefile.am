#
# Copyright (C) LavaLite Contributors
#
# lstools/Makefile.am

include $(top_srcdir)/Make.common
COMMON_LIBS = ../lib/libll.a

LDADD = $(COMMON_LIBS)

bin_PROGRAMS = lshosts lsid lsinfo \
	lsloadadj lsload lsplace

# Command source and linker
lshosts_SOURCES    = lshosts.c
lsid_SOURCES       = lsid.c
lsinfo_SOURCES     = lsinfo.c
lsloadadj_SOURCES  = lsloadadj.c
lsload_SOURCES     = lsload.c load.c
lsplace_SOURCES = lsplace.c

