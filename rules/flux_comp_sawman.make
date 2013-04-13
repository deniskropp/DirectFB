$(builddir)/%.cpp $(builddir)/%.h: $(srcdir)/%.flux
	fluxcomp -i --call-mode --static-args-bytes=FLUXED_ARGS_BYTES --dispatch-error-abort $<
