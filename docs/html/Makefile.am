EXTRA_DIST = cimlogo.png directfb.png

docs_headers = \
	$(top_srcdir)/include/directfb_keyboard.h \
	$(top_srcdir)/include/directfb.h

all-local: stamp-docs

stamp-docs: $(top_srcdir)/tools/gendoc.pl $(docs_headers) Makefile.am
	rootme=`pwd`; \
	$(PERL) $(top_srcdir)/tools/gendoc.pl $(docs_headers) \
	&& echo timestamp > $$rootme/stamp-docs

distclean-local:
	rm -f *.html stamp-docs
