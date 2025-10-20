# Install locations: prefer docdir/pkgdocdir for docs (not datadir)
pkgdocdir = $(docdir)/$(PACKAGE)/projects

# Ship the source .tex in the tarball
EXTRA_DIST = lavalite.tex lavalite-admin.tex

# If pdflatex exists, build a PDF and install it as documentation.
if HAVE_PDFLATEX
# Build PDFs by default (make / make all)
all-local: lavalite.pdf lavalite-admin.pdf

# Build hook that runs alongside Automake's own "pdf" target
pdf-local: lavalite.pdf

# Pattern-free explicit rule for your single file
lavalite.pdf: lavalite.tex
	$(AM_V_GEN)$(PDFLATEX) -interaction=nonstopmode -halt-on-error \
	-output-directory=$(builddir) $< >/dev/null
lavalite-admin.pdf: lavalite-admin.tex
	$(AM_V_GEN)$(PDFLATEX) -interaction=nonstopmode -halt-on-error \
	-output-directory=$(builddir) $< >/dev/null

# Install the built PDF with the other docs
dist_doc_DATA = lavalite.pdf

# Clean generated LaTeX artifacts
CLEANFILES = *.pdf *.aux *.log *.out *.toc
else
# No pdflatex present: keep targets valid but do nothing
pdf-local:
	@echo "pdflatex not found; skipping PDF build." >&2
dist_doc_DATA =
endif

