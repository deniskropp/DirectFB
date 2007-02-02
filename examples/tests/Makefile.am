SUBDIRS = data

INCLUDES = \
	-I$(top_srcdir)/include	\
	@DIRECTFB_CFLAGS@

LDADD = \
	$(top_builddir)/++dfb/lib++dfb.la	\
	@DIRECTFB_LIBS@

noinst_PROGRAMS = images


images_SOURCES = images.cpp
