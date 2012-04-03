FLUX_ARGS ?= -i --include-prefix=core --call-mode

$(builddir)/%.cpp $(builddir)/%.h: $(srcdir)/%.flux
	fluxcomp $(FLUX_ARGS) $<
