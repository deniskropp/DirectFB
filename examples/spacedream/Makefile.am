## Makefile.am for DirectFB/examples/spacedream

spacedreamdatadir = $(DATADIR)/examples/spacedream

spacedreamdata_DATA = \
	star1.png	\
	star2.png	\
	star3.png	\
	star4.png

EXTRA_DIST = $(spacedreamdata_DATA)

INCLUDES = \
	-I$(top_srcdir)/include

CFLAGS = $(DFB_CFLAGS)

LIBADDS = \
	$(top_builddir)/src/libdirectfb.la \
	$(DFB_LDFLAGS) -lm

fontsdatadir = $(DATADIR)/fonts

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@/examples/spacedream\" \
	-DFONT=\"$(fontsdatadir)/decker.ttf\"

bin_PROGRAMS = spacedream

spacedream_SOURCES = main.c
spacedream_LDADD   = $(LIBADDS)

.PHONY: files

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
	@for subdir in $(SUBDIRS); do \
	  files=`cd $$subdir; $(MAKE) files | grep -v "make\[[1-9]\]"`; \
	  for file in $$files; do \
	    echo $$subdir/$$file; \
	  done; \
	done
