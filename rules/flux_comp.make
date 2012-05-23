FLUX_ARGS ?= -i --include-prefix=core --call-mode --object-ptrs

$(builddir)/%.cpp $(builddir)/%.h: $(srcdir)/%.flux
	$(FLUXCOMP) $(FLUX_ARGS) $<
