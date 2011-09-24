$(builddir)/%.c $(builddir)/%.h: $(srcdir)/%.flux
	fluxcomp -c -i --include-prefix=core $<
