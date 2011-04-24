EXTRA_DIST = dfb_logo-alpha.png

docs_headers = \
	$(top_srcdir)/include/directfb.h \
	$(top_srcdir)/include/directfb_keyboard.h \
	$(top_srcdir)/include/directfbgl.h \
	$(top_srcdir)/include/directfbgl2.h \
	$(top_srcdir)/include/directfb_water.h \
	$(top_srcdir)/include/directfb_windows.h

all-local: stamp-docs

stamp-docs: $(top_srcdir)/tools/gendoc.pl $(docs_headers) Makefile.am
	rootme=`pwd`; \
	$(PERL) $(top_srcdir)/tools/gendoc.pl DirectFB $(DIRECTFB_VERSION) $(docs_headers) \
	&& echo timestamp > $$rootme/stamp-docs

distclean-local:
	rm -f *.html stamp-docs
