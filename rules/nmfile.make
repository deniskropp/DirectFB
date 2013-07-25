if BUILD_SHARED
if ENABLE_TRACE

NM ?= nm


LIBS_TO_NMFILE ?= $(LTLIBRARIES)

NMEDLIB = $(LIBS_TO_NMFILE:.la=-$(LT_RELEASE).so.$(LT_BINARY))

install-data-local: install-libLTLIBRARIES
	mkdir -p -- "$(DESTDIR)$(libdir)"
	$(NM) -nC "$(DESTDIR)$(libdir)/$(NMEDLIB)" > "$(DESTDIR)$(libdir)/nm-n.$(NMEDLIB)"

endif
endif
