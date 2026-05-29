# docs/man/Makefile.am
PANDOC  = pandoc
mandir  = $(prefix)/share/man

MAN1 = man1/bhosts.1 \
       man1/bjobs.1 \
       man1/bkill.1 \
       man1/bgroups.1 \
       man1/bqueues.1 \
       man1/bsub.1 \
       man1/btokens.1 \
       man1/bhist.1 \
       man1/bpriority.1 \
       man1/bmove.1

MAN5 = man5/ll.conf.5 \
       man5/llb.hosts.5 \
       man5/llb.queues.5

MAN7 = man7/lavalite.7

MAN8 = man8/mbd.8 \
       man8/sbd.8

man1_MANS = $(MAN1)
man5_MANS = $(MAN5)
man7_MANS = $(MAN7)
man8_MANS = $(MAN8)

EXTRA_DIST = \
	markdown/bhosts.1.md \
	markdown/bjobs.1.md \
	markdown/bkill.1.md \
	markdown/bgroups.1.md \
	markdown/bqueues.1.md \
	markdown/bsub.1.md \
	markdown/btokens.1.md \
	markdown/bhist.1.md \
	markdown/bpriority.1.md \
	markdown/bmove.1.md \
	markdown/ll.conf.5.md \
	markdown/llb.hosts.5.md \
	markdown/llb.queues.5.md \
	markdown/lavalite.7.md \
	markdown/mbd.8.md \
	markdown/sbd.8.md

BUILT_SOURCES = $(MAN1) $(MAN5) $(MAN7) $(MAN8)
CLEANFILES    = $(MAN1) $(MAN5) $(MAN7) $(MAN8)

man1/bhosts.1:   markdown/bhosts.1.md
man1/bjobs.1:    markdown/bjobs.1.md
man1/bkill.1:    markdown/bkill.1.md
man1/bgroups.1:   markdown/bgroups.1.md
man1/bqueues.1:  markdown/bqueues.1.md
man1/bsub.1:     markdown/bsub.1.md
man1/btokens.1:  markdown/btokens.1.md
man1/bhist.1:    markdown/bhist.1.md
man1/bpriority.1:  markdown/bpriority.1.md
man1/bmove.1:    markdown/bmove.1.md

man5/ll.conf.5:    markdown/ll.conf.5.md
man5/llb.hosts.5:  markdown/llb.hosts.5.md
man5/llb.queues.5: markdown/llb.queues.5.md

man7/lavalite.7: markdown/lavalite.7.md

man8/mbd.8: markdown/mbd.8.md
man8/sbd.8: markdown/sbd.8.md

$(MAN1) $(MAN5) $(MAN7) $(MAN8):
	$(PANDOC) -s -t man $< -o $@
