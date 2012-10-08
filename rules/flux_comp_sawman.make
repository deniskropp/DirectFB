$(builddir)/%.cpp $(builddir)/%.h: $(srcdir)/%.flux
	fluxcomp --static-args-bytes=FLUXED_ARGS_BYTES $<
