## Makefile.am for FusionSound/interfaces/IfusionsoundMusicProvider

ifusionsoundmusicproviderdir = $(MODULEDIR)/interfaces/IFusionSoundMusicProvider

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	$(DFB_CFLAGS)			\
	$(VORBISFILE_CFLAGS)	\
	$(CDDB_CFLAGS)


if BUILD_TIMIDITY
SUBDIRS = timidity
TIMIDITY_PROVIDER_LTLIB = libifusionsoundmusicprovider_timidity.la
endif

if BUILD_WAVE
WAVE_PROVIDER_LTLIB = libifusionsoundmusicprovider_wave.la
endif

if BUILD_VORBIS
VORBIS_PROVIDER_LTLIB = libifusionsoundmusicprovider_vorbis.la
endif

if BUILD_MAD
MAD_PROVIDER_LTLIB = libifusionsoundmusicprovider_mad.la
endif

if BUILD_CDDA
CDDA_PROVIDER_LTLIB = libifusionsoundmusicprovider_cdda.la
endif

if BUILD_PLAYLIST
PLAYLIST_PROVIDER_LTLIB = libifusionsoundmusicprovider_playlist.la
endif

ifusionsoundmusicprovider_LTLIBRARIES =	\
	$(TIMIDITY_PROVIDER_LTLIB)	\
	$(WAVE_PROVIDER_LTLIB)		\
	$(VORBIS_PROVIDER_LTLIB)		\
	$(MAD_PROVIDER_LTLIB)		\
	$(CDDA_PROVIDER_LTLIB)		\
	$(PLAYLIST_PROVIDER_LTLIB)


libifusionsoundmusicprovider_timidity_la_SOURCES = ifusionsoundmusicprovider_timidity.c

libifusionsoundmusicprovider_timidity_la_LIBADD = timidity/libtimidity.la

libifusionsoundmusicprovider_timidity_la_LDFLAGS = -avoid-version -module


libifusionsoundmusicprovider_wave_la_SOURCES = ifusionsoundmusicprovider_wave.c

libifusionsoundmusicprovider_wave_la_LIBADD =

libifusionsoundmusicprovider_wave_la_LDFLAGS = -avoid-version -module


libifusionsoundmusicprovider_vorbis_la_SOURCES = ifusionsoundmusicprovider_vorbis.c

libifusionsoundmusicprovider_vorbis_la_LIBADD = $(VORBISFILE_LIBS)

libifusionsoundmusicprovider_vorbis_la_LDFLAGS = -avoid-version -module


libifusionsoundmusicprovider_mad_la_SOURCES = ifusionsoundmusicprovider_mad.c

libifusionsoundmusicprovider_mad_la_LIBADD = $(MAD_LIBS)

libifusionsoundmusicprovider_mad_la_LDFLAGS = -avoid-version -module


libifusionsoundmusicprovider_cdda_la_SOURCES = ifusionsoundmusicprovider_cdda.c

libifusionsoundmusicprovider_cdda_la_LIBADD = $(CDDB_LIBS)

libifusionsoundmusicprovider_cdda_la_LDFLAGS = -avoid-version -module


libifusionsoundmusicprovider_playlist_la_SOURCES = ifusionsoundmusicprovider_playlist.c

libifusionsoundmusicprovider_playlist_la_LDFLAGS = -avoid-version -module

