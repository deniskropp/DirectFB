## Makefile.am for DirectFB/examples/pixbuf-demo

examplesdatadir = $(DATADIR)/examples

examplesdata_DATA = \
	apple-red.png		\
	background.jpg		\
	gnome-applets.png	\
	gnome-calendar.png	\
	gnome-foot.png		\
	gnome-gimp.png		\
	gnome-gmush.png		\
	gnome-gsame.png		\
	gnu-keys.png

EXTRA_DIST = $(examplesdata_DATA)

INCLUDES = \
	-I$(top_srcdir)/include

CFLAGS = $(DFB_CFLAGS)

LIBADDS = \
	$(top_builddir)/src/libdirectfb.la \
	$(DFB_LDFLAGS) -lm

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@/examples\"

bin_PROGRAMS = df_neo

df_neo_SOURCES = df_neo.c
df_neo_LDADD  = $(LIBADDS)

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
