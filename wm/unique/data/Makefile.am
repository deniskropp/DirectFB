## Makefile.am for DirectFB/wm/unique/data

uniquedatadir = $(DATADIR)/unique

all-local: foo.h

clean-local:
	rm -f foo.h

EXTRA_DIST = \
	foo_n.png	\
	foo_ne.png	\
	foo_e.png	\
	foo_se.png	\
	foo_s.png	\
	foo_sw.png	\
	foo_w.png	\
	foo_nw.png

if CROSS_COMPILING
  directfb_csource = $(DIRECTFB_CSOURCE)
else
if BUILD_TOOLS
  directfb_csource = $(top_builddir)/tools/directfb-csource
else
  directfb_csource = $(DIRECTFB_CSOURCE)
endif
endif

foo.h: $(EXTRA_DIST)
	$(directfb_csource) --name=foo $(EXTRA_DIST) > foo.h
