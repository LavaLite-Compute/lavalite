#
#  Copyright (C) LavaLite Contributors
#

# Make sources see headers under include/ and the generated config.h
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src docs

# Install default working directories
install-data-local:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)
	$(MKDIR_P) $(DESTDIR)$(localstatedir)/log
	$(MKDIR_P) $(DESTDIR)$(localstatedir)/work
	$(MKDIR_P) $(DESTDIR)$(localstatedir)/work/info

