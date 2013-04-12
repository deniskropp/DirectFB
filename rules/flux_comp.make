FLUX_ARGS ?= -i --include-prefix=core --call-mode --object-ptrs --static-args-bytes=FLUXED_ARGS_BYTES --dispatch-error-abort

$(builddir)/%.cpp $(builddir)/%.h: $(srcdir)/%.flux
	$(FLUXCOMP) $(FLUX_ARGS) $<
