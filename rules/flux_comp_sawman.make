$(builddir)/%.cpp $(builddir)/%.h: $(srcdir)/%.flux
	fluxcomp $<
