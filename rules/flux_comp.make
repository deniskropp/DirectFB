$(builddir)/%.c $(builddir)/%.h: $(srcdir)/%.flux
	fluxcomp -c --include-prefix=core $<
