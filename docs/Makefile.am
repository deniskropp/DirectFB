## Makefile.am for DirectFB/docs

docs_headers = $(top_srcdir)/include/directfb.h

EXTRA_DIST = html/cimlogo.png html/directfb.png

all-local: stamp-docs

stamp-docs: $(top_srcdir)/tools/gendoc.pl $(docs_headers) Makefile.am
	rootme=`pwd`; \
	cd $(srcdir) && $(PERL) $(top_srcdir)/tools/gendoc.pl $(docs_headers) \
	&& echo timestamp > $$rootme/stamp-docs
