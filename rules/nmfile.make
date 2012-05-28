if BUILD_SHARED
if ENABLE_TRACE

NM ?= nm

LIBTONM = $(LTLIBRARIES:.la=-$(LT_RELEASE).so.$(LT_BINARY))

install-data-local: install-libLTLIBRARIES
	mkdir -p -- "$(DESTDIR)$(libdir)"
	$(NM) -nC "$(DESTDIR)$(libdir)/$(LIBTONM)" > "$(DESTDIR)$(libdir)/nm-n.$(LIBTONM)"

endif
endif
