mandir = $(prefix)/share/man

man1_MANS = man1/lseligible.1 \
            man1/lsfbase.1 \
            ...

man5_MANS = ...
man7_MANS = ...
man8_MANS = ...

# Markdown sources
MD_SOURCES = \
    markdown/lseligible.1.md \
    markdown/lsfbase.1.md \
    ...

# Pattern rule: .md → .1/.5/.7/.8
man1/%.1: docs/%.1.md
    pandoc -s -t man $< -o $@

man5/%.5: docs/%.5.md
    pandoc -s -t man $< -o $@

man7/%.7: docs/%.7.md
    pandoc -s -t man $< -o $@

man8/%.8: docs/%.8.md
    pandoc -s -t man $< -o $@

# Ensure man pages are rebuilt when Markdown changes
BUILT_SOURCES = $(man1_MANS) $(man5_MANS) $(man7_MANS) $(man8_MANS)

EXTRA_DIST = $(MD_SOURCES)

