$(builddir)/%.cpp $(builddir)/%.h: $(srcdir)/%.flux
	fluxcomp --include-prefix=core $<
