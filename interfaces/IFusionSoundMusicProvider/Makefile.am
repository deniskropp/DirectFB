## Makefile.am for FusionSound/interfaces/IfusionsoundMusicProvider

SUBDIRS = timidity

ifusionsoundmusicproviderdir = $(MODULEDIR)/interfaces/IFusionSoundMusicProvider

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	$(DFB_CFLAGS)			\
	$(VORBISFILE_CFLAGS)


TIMIDITY_PROVIDER_LTLIB = libifusionsoundmusicprovider_timidity.la

if VORBIS
VORBIS_PROVIDER_LTLIB = libifusionsoundmusicprovider_vorbis.la
endif


ifusionsoundmusicprovider_LTLIBRARIES =	\
	$(TIMIDITY_PROVIDER_LTLIB)	\
	$(VORBIS_PROVIDER_LTLIB)


libifusionsoundmusicprovider_timidity_la_SOURCES = ifusionsoundmusicprovider_timidity.c

libifusionsoundmusicprovider_timidity_la_LIBADD = timidity/libtimidity.la

libifusionsoundmusicprovider_timidity_la_LDFLAGS = -avoid-version -module


libifusionsoundmusicprovider_vorbis_la_SOURCES = ifusionsoundmusicprovider_vorbis.c

libifusionsoundmusicprovider_vorbis_la_LIBADD = $(VORBISFILE_LIBS)

libifusionsoundmusicprovider_vorbis_la_LDFLAGS = -avoid-version -module

