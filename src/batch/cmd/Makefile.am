#
# Copyright (C) LavaLite Contributors
#
include $(top_srcdir)/Make.common

COMMON_LIBS = ../lib/libllbat.a ../../base/lib/libllbase.a -lm $(OPENSSL_LIBS)

LDADD = $(COMMON_LIBS)

bin_PROGRAMS = bhosts bjobs bkill bqueues bsub bgroups btokens bhist \
	       bmove bpriority bservices

# Source and linker definitions
bhosts_SOURCES    = bhosts.c $(COMMON_CMD_SOURCES)
bjobs_SOURCES     = bjobs.c $(COMMON_CMD_SOURCES)
bkill_SOURCES     = bkill.c $(COMMON_CMD_SOURCES)
bgroups_SOURCES   = bgroups.c $(COMMON_CMD_SOURCES)
bqueues_SOURCES   = bqueues.c $(COMMON_CMD_SOURCES)
bsub_SOURCES      = bsub.c $(COMMON_CMD_SOURCES)
btokens_SOURCES   = btokens.c $(COMMON_CMD_SOURCES)
bhist_SOURCES   = bhist.c $(COMMON_CMD_SOURCES)
bmove_SOURCES   = bmove.c $(COMMON_CMD_SOURCES)
bpriority_SOURCES   = bpriority.c $(COMMON_CMD_SOURCES)
bservices_SOURCES   = bservices.c $(COMMON_CMD_SOURCES)

EXTRA_DIST = Make.common


# bhist reads the manifest as the mbd group, see etc/post-install.sh.
# install replaces it root:root 755, put the group and setgid back
# when root and ll.conf is installed. chgrp first, it clears setgid.
install-exec-hook:
	@conf=$(DESTDIR)$(sysconfdir)/ll.conf; \
	user=`grep '^LL_MBD_USER' $$conf 2>/dev/null | cut -d= -f2`; \
	group=`id -gn "$$user" 2>/dev/null`; \
	bhist=$(DESTDIR)$(bindir)/bhist; \
	if [ -n "$$user" ] && [ -n "$$group" ] && [ "`id -u`" = 0 ]; then \
	    chgrp "$$group" $$bhist && chmod 2755 $$bhist && \
	    echo "bhist: setgid group $$group"; \
	else \
	    echo "bhist: not setgid, run post-install.sh as root"; \
	fi
