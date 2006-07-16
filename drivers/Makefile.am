# Makefile.am for FusionSound/drivers

INCLUDES = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src	    \
	$(DFB_CFLAGS)	\
	$(ALSA_CFLAGS)

driversdir = $(MODULEDIR)/snddrivers

if BUILD_OSS
OSS_DRIVER = libfusionsound_oss.la
endif

if BUILD_ALSA
ALSA_DRIVER = libfusionsound_alsa.la
endif

if BUILD_WAVE
WAVE_DRIVER = libfusionsound_wave.la
endif

drivers_LTLIBRARIES = \
	$(OSS_DRIVER)	\
	$(ALSA_DRIVER)	\
	$(WAVE_DRIVER)


libfusionsound_oss_la_SOURCES = oss.c

libfusionsound_oss_la_LIBADD = ../src/libfusionsound.la

libfusionsound_oss_la_LDFLAGS = -export-dynamic -avoid-version -module


libfusionsound_alsa_la_SOURCES = alsa.c

libfusionsound_alsa_la_LIBADD = ../src/libfusionsound.la $(ALSA_LIBS)

libfusionsound_alsa_la_LDFLAGS = -export-dynamic -avoid-version -module


libfusionsound_wave_la_SOURCES = wave.c

libfusionsound_wave_la_LIBADD = ../src/libfusionsound.la

libfusionsound_wave_la_LDFLAGS = -export-dynamic -avoid-version -module

