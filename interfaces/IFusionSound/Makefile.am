# Makefile.am for FusionSound/interfaces/IFusionSound

ifusionsounddir = $(MODULEDIR)/interfaces/IFusionSound

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	$(FUSION_CFLAGS)

ifusionsound_LTLIBRARIES = libifusionsound.la

libifusionsound_la_SOURCES = ifusionsound_module.c

libifusionsound_la_LIBADD = $(FUSION_LIBS) ../../src/libfusionsound.la

libifusionsound_la_LDFLAGS = -avoid-version -module
