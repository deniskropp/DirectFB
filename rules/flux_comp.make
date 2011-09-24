$(builddir)/%.cpp $(builddir)/%.h: $(srcdir)/%.flux
	fluxcomp -i --include-prefix=core $<
