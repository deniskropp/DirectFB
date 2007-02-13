SUBDIRS = tests


INCLUDES = \
	-I$(top_srcdir)/include	\
	@DIRECTFB_CFLAGS@

LDADD = \
	$(top_builddir)/++dfb/lib++dfb.la	\
	@DIRECTFB_LIBS@


noinst_PROGRAMS = simple

bin_PROGRAMS = dfbshow dfbswitch dfbplay


simple_SOURCES = \
	dfbapp.cpp	\
	dfbapp.h	\
	simple.cpp

dfbshow_SOURCES = \
	dfbapp.cpp	\
	dfbapp.h	\
	dfbimage.cpp	\
	dfbimage.h	\
	dfbshow.cpp

dfbplay_SOURCES = \
	dfbplay.cpp

dfbswitch_SOURCES = dfbswitch.cpp
