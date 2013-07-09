## Makefile.am for examples/++dfb
SUBDIRS = data


# for DFB_BASE_LIBS etc...
include $(top_srcdir)/rules/libs_deps.make


INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/include/++dfb \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/++dfb \
	-I$(top_srcdir)/lib

LIBADDS = $(DFB_BASE_LIBS) $(libppdfb)


noinst_PROGRAMS = images

images_SOURCES = images.cpp
images_LDADD = $(LIBADDS)
