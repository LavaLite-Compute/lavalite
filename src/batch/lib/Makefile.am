#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

lib_LIBRARIES = libllbat.a

libllbat_a_SOURCES =  rpc.c submit.c api.c log.c wire.c jobscript.c \
		      history.c dependency.c sign.c

# sign.c runs the installed llsign
AM_CPPFLAGS += -DLL_LIBEXECDIR='"$(libexecdir)"'
