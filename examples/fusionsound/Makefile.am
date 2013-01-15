## Makefile.am for FusionSound/examples
INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/lib/fusionsound \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib     \
	-I$(top_srcdir)/lib/fusionsound


LIBADDS = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/lib/fusionsound/libfusionsound.la

EXTRA_DIST = \
	test.wav	\
	test2.wav

noinst_PROGRAMS = fs_advanced meter music music_loader fs_simple stream

fs_advanced_SOURCES = fs_advanced.c loader.c loader.h
fs_advanced_LDADD   = $(LIBADDS) -lm

meter_SOURCES = meter.c
meter_LDADD   = $(LIBADDS)

music_SOURCES = music.c
music_LDADD   = $(LIBADDS)

music_loader_SOURCES = music_loader.c
music_loader_LDADD   = $(LIBADDS)

fs_simple_SOURCES = fs_simple.c loader.c loader.h
fs_simple_LDADD   = $(LIBADDS)

stream_SOURCES = stream.c
stream_LDADD   = $(LIBADDS) -lm
