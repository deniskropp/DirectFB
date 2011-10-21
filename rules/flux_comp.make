FLUX_ARGS ?= -i --include-prefix=core

$(builddir)/%.cpp $(builddir)/%.h: $(srcdir)/%.flux
	fluxcomp $(FLUX_ARGS) $<
