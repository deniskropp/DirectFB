# Makefile.am for FusionSound/interfaces/IFusionSound

ifusionsounddir = $(MODULEDIR)/interfaces/IFusionSound

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/src 	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusionsound	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusionsound	\
	$(FUSION_CFLAGS)

ifusionsound_LTLIBRARIES = libifusionsound.la

libifusionsound_la_SOURCES = ifusionsound_module.c

libifusionsound_la_LIBADD = $(FUSION_LIBS) $(top_builddir)/lib/fusionsound/libfusionsound.la

libifusionsound_la_LDFLAGS = -avoid-version -module
