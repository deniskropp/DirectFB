SUBDIRS = tests
## Makefile.am for FusionSound/examples
INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/include/++dfb \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/++dfb \
	-I$(top_srcdir)/lib


# for DFB_BASE_LIBS etc...
include $(top_srcdir)/rules/libs_deps.make


LIBADDS = $(DFB_BASE_LIBS) $(libppdfb)


noinst_PROGRAMS = simple

bin_PROGRAMS = dfbshow dfbswitch dfbplay


simple_SOURCES = \
	dfbapp.cpp	\
	dfbapp.h	\
	simple.cpp

simple_LDADD = $(LIBADDS)

dfbshow_SOURCES = \
	dfbapp.cpp	\
	dfbapp.h	\
	dfbimage.cpp	\
	dfbimage.h	\
	dfbshow.cpp

dfbshow_LDADD = $(LIBADDS)

dfbplay_SOURCES = \
	dfbplay.cpp

dfbplay_LDADD = $(LIBADDS)

dfbswitch_SOURCES = dfbswitch.cpp
dfbswitch_LDADD = $(LIBADDS)
