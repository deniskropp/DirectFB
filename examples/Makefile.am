## Makefile.am for FusionSound/examples

INCLUDES = -I$(top_srcdir)/include $(DFB_CFLAGS) $(LITE_CFLAGS)

LIBADDS = $(DFB_LIBS) -lm

EXTRA_DIST = \
	test.wav	\
	test2.wav

if HAVE_LITE
LITE_EXAMPLES = player
else
LITE_EXAMPLES =
endif

noinst_PROGRAMS = advanced music simple stream $(LITE_EXAMPLES)

advanced_SOURCES = advanced.c loader.c loader.h
advanced_LDADD   = $(LIBADDS)

music_SOURCES = music.c
music_LDADD   = $(LIBADDS)

simple_SOURCES = simple.c loader.c loader.h
simple_LDADD   = $(LIBADDS)

stream_SOURCES = stream.c
stream_LDADD   = $(LIBADDS)

player_SOURCES = player.c loader.c loader.h
player_LDADD   = $(LIBADDS) $(LITE_LIBS)
