## Makefile.am for DirectFB/examples

SUBDIRS = df_knuckles df_neo spacedream

miscdatadir = $(DATADIR)

miscdata_DATA = \
	cursor.dat

examplesdatadir = $(DATADIR)/examples

examplesdata_DATA = \
	colorkeyed.png		\
	desktop.png		\
	tux.png			\
	destination_mask.png	\
	melted.png		\
	pngtest.png		\
	pngtest2.png		\
	meter.png		\
	intro.png		\
	shot.png		\
	dfblogo.png		\
	convergence.png		\
	wood_andi.jpg		\
	smokey_light.jpg

fontsdatadir = $(DATADIR)/fonts

fontsdata_DATA = decker.ttf font.data

EXTRA_DIST = $(miscdata_DATA) $(examplesdata_DATA) $(fontsdata_DATA)

INCLUDES = \
	-I$(top_srcdir)/include

CFLAGS = \
	$(DFB_CFLAGS)	\
	-DFONT=\"$(fontsdatadir)/decker.ttf\"

LIBADDS = \
	$(top_builddir)/src/libdirectfb.la \
	$(DFB_LDFLAGS)

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"

bin_PROGRAMS = df_fire df_dok df_andi df_window df_particle df_video_particle df_joystick df_video df_porter df_stress pss

df_fire_SOURCES = df_fire.c
df_fire_LDADD = $(LIBADDS)

df_window_SOURCES = df_window.c 
df_window_LDADD  = $(LIBADDS) $(AVIFILE_LIBS) -lm

df_video_SOURCES = df_video.c 
df_video_LDADD  = $(LIBADDS) $(AVIFILE_LIBS) -lm

df_dok_SOURCES = df_dok.c 
df_dok_LDADD  = $(LIBADDS)

df_andi_SOURCES = df_andi.c 
df_andi_LDADD  = $(LIBADDS)

df_particle_SOURCES = df_particle.c 
df_particle_LDADD  = $(LIBADDS) -lm

df_video_particle_SOURCES = df_video_particle.c 
df_video_particle_LDADD  = $(LIBADDS) $(AVIFILE_LIBS) -lm

df_joystick_SOURCES = df_joystick.c 
df_joystick_LDADD  = $(LIBADDS) -lm

df_porter_SOURCES = df_porter.c 
df_porter_LDADD  = $(LIBADDS)

df_stress_SOURCES = df_stress.c
df_stress_LDADD = $(LIBADDS)

pss_SOURCES = pss.c
pss_LDADD = $(LIBADDS) -lm

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
