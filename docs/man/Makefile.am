# docs/man/Makefile.am
PANDOC  = pandoc
mandir  = $(prefix)/share/man

MAN1 = man1/bhosts.1 \
       man1/bjobs.1 \
       man1/bkill.1 \
       man1/bmgroup.1 \
       man1/bqueues.1 \
       man1/bsub.1 \
       man1/bswitch.1 \
       man1/lshosts.1 \
       man1/lsid.1 \
       man1/lsload.1

MAN5 = man5/lsb.acct.5 \
       man5/lsb.events.5 \
       man5/lsb.hosts.5 \
       man5/lsb.queues.5 \
       man5/lsf.cluster.5 \
       man5/lsf.conf.5

MAN8 = man8/lim.8 \
       man8/mbd.8 \
       man8/sbd.8

man1_MANS = $(MAN1)
man5_MANS = $(MAN5)
man8_MANS = $(MAN8)

EXTRA_DIST = \
	markdown/bhosts.1.md \
	markdown/bjobs.1.md \
	markdown/bkill.1.md \
	markdown/bmgroup.1.md \
	markdown/bqueues.1.md \
	markdown/bsub.1.md \
	markdown/bswitch.1.md \
	markdown/lshosts.1.md \
	markdown/lsid.1.md \
	markdown/lsload.1.md \
	markdown/lsb.acct.5.md \
	markdown/lsb.events.5.md \
	markdown/lsb.hosts.5.md \
	markdown/lsb.queues.5.md \
	markdown/lsf.cluster.5.md \
	markdown/lsf.conf.5.md \
	markdown/lim.8.md \
	markdown/mbd.8.md \
	markdown/sbd.8.md


BUILT_SOURCES = $(MAN1) $(MAN5) $(MAN8)
CLEANFILES    = $(MAN1) $(MAN5) $(MAN8)

man1/bhosts.1:   markdown/bhosts.1.md
man1/bjobs.1:    markdown/bjobs.1.md
man1/bkill.1:    markdown/bkill.1.md
man1/bmgroup.1:  markdown/bmgroup.1.md
man1/bqueues.1:  markdown/bqueues.1.md
man1/bsub.1:     markdown/bsub.1.md
man1/bswitch.1:  markdown/bswitch.1.md
man1/lshosts.1:  markdown/lshosts.1.md
man1/lsid.1:     markdown/lsid.1.md
man1/lsload.1:   markdown/lsload.1.md

man5/lsb.acct.5:    markdown/lsb.acct.5.md
man5/lsb.events.5:  markdown/lsb.events.5.md
man5/lsb.hosts.5:   markdown/lsb.hosts.5.md
man5/lsb.queues.5:  markdown/lsb.queues.5.md
man5/lsf.cluster.5: markdown/lsf.cluster.5.md
man5/lsf.conf.5:    markdown/lsf.conf.5.md

man8/lim.8: markdown/lim.8.md
man8/mbd.8: markdown/mbd.8.md
man8/sbd.8: markdown/sbd.8.md

$(MAN1) $(MAN5) $(MAN8):
	$(PANDOC) -s -t man $< -o $@
