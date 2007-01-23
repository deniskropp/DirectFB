if BUILD_SHARED
if ENABLE_TRACE

LIBTONM = $(LTLIBRARIES:.la=-$(LT_RELEASE).so.$(LT_CURRENT))

install-data-local:
	nm -n .libs/$(LIBTONM) > $(DESTDIR)$(libdir)/nm-n.$(LIBTONM)

endif
endif
