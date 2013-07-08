## Makefile.am for examples/++dfb
SUBDIRS = data

INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/include/++dfb \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/++dfb \
	-I$(top_srcdir)/lib

LIBADDS = \
	$(top_builddir)/src/libdirectfb.la \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/lib/++dfb/lib++dfb.la


noinst_PROGRAMS = images

images_SOURCES = images.cpp
images_LDADD = $(LIBADDS)
