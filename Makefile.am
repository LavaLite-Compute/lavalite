#
#  Copyright (C) LavaLite Contributors
#

# Make sources see headers under include/ and the generated config.h
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include src docs
EXTRA_DIST = Make.common

# systemd units, copied to /etc/systemd/system by the admin
systemddir = $(prefix)/lib/systemd
dist_systemd_DATA = etc/lavalite-mbd.service etc/lavalite-sbd.service \
		    etc/lavalite-sbd-sim.service

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)
	$(MKDIR_P) $(DESTDIR)$(localstatedir)/log
	$(MKDIR_P) $(DESTDIR)$(localstatedir)/state
