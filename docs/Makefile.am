#
docdir = $(prefix)/share/man/man1

dist_doc_DATA = \
    doc/man/lavalite.1 \
    doc/man/lavalite.conf.5 \
    doc/man/lavalite-submit.1

install-docs:
    @echo "Installing man pages to $(docdir)"
    @mkdir -p $(docdir)
    @cp doc/man/*.1 $(docdir)
    @cp doc/man/*.5 $(docdir)

