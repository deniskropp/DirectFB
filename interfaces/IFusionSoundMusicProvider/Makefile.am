## Makefile.am for FusionSound/interfaces/IfusionsoundMusicProvider

SUBDIRS = timidity

ifusionsoundmusicproviderdir = $(MODULEDIR)/interfaces/IFusionSoundMusicProvider


TIMIDITY_PROVIDER_LTLIB = libifusionsoundmusicprovider_timidity.la

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
        $(DFB_CFLAGS)

ifusionsoundmusicprovider_LTLIBRARIES =	\
	$(TIMIDITY_PROVIDER_LTLIB)

libifusionsoundmusicprovider_timidity_la_SOURCES = ifusionsoundmusicprovider_timidity.c

libifusionsoundmusicprovider_timidity_la_LIBADD = timidity/libtimidity.la

libifusionsoundmusicprovider_timidity_la_LDFLAGS = -avoid-version -module
