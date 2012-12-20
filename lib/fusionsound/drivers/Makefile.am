# Makefile.am for lib/fusionsound/drivers

INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusionsound	\
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusionsound	\
	$(FUSION_CFLAGS)	\
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
	libfusionsound_dummy.la	\
	$(OSS_DRIVER)	\
	$(ALSA_DRIVER)	\
	$(WAVE_DRIVER)


libfusionsound_dummy_la_SOURCES = dummy.c

libfusionsound_dummy_la_LIBADD = $(top_builddir)/lib/direct/libdirect.la $(top_builddir)/lib/fusionsound/libfusionsound.la


libfusionsound_dummy_la_LDFLAGS = -export-dynamic -avoid-version -module


libfusionsound_oss_la_SOURCES = oss.c

libfusionsound_oss_la_LIBADD = $(top_builddir)/lib/direct/libdirect.la $(top_builddir)/lib/fusionsound/libfusionsound.la


libfusionsound_oss_la_LDFLAGS = -export-dynamic -avoid-version -module


libfusionsound_alsa_la_SOURCES = alsa.c

libfusionsound_alsa_la_LIBADD = $(top_builddir)/lib/direct/libdirect.la $(top_builddir)/lib/fusionsound/libfusionsound.la $(ALSA_LIBS)

libfusionsound_alsa_la_LDFLAGS = -export-dynamic -avoid-version -module


libfusionsound_wave_la_SOURCES = wave.c

libfusionsound_wave_la_LIBADD = $(top_builddir)/lib/direct/libdirect.la $(top_builddir)/lib/fusionsound/libfusionsound.la

libfusionsound_wave_la_LDFLAGS = -export-dynamic -avoid-version -module

