## Makefile.am for DirectFB/examples/df_knuckles

INCLUDES = \
	-I$(top_srcdir)/include

CFLAGS = $(DFB_CFLAGS)

LIBADDS = \
	$(top_builddir)/src/libdirectfb.la \
	$(DFB_LDFLAGS)

bin_PROGRAMS = df_knuckles

df_knuckles_SOURCES = df_knuckles.c matrix.c matrix.h skull.h
df_knuckles_LDADD  = $(LIBADDS) -lm

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
