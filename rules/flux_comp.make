FLUX_ARGS ?= -c -i --include-prefix=core

$(builddir)/%.c $(builddir)/%.h: $(srcdir)/%.flux
	fluxcomp $(FLUX_ARGS) $<
