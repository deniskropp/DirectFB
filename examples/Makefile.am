## Makefile.am for FusionSound/examples

INCLUDES = -I$(top_srcdir)/include $(DFB_CFLAGS)

LIBADDS = $(DFB_LIBS) -lm

EXTRA_DIST = \
	test.wav	\
	test2.wav

noinst_PROGRAMS = advanced simple stream

advanced_SOURCES = advanced.c
advanced_LDADD   = $(LIBADDS)

simple_SOURCES = simple.c
simple_LDADD   = $(LIBADDS)

stream_SOURCES = stream.c
stream_LDADD   = $(LIBADDS)
