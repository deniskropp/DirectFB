## Makefile.am for DirectFB/tools

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

AM_CFLAGS = $(DFB_CFLAGS)


if DIRECTFB_CSOURCE
DFB_CSOURCE = directfb-csource

directfb_csource_SOURCES = directfb-csource.c
directfb_csource_LDADD   = $(LIBPNG)
endif

bin_PROGRAMS = $(DFB_CSOURCE)

noinst_PROGRAMS = \
	dfbdump 	\
	fusion_bench	\
	raw15toraw24	\
	raw16toraw24	\
	raw32toraw24

dfbdump_SOURCES = dfbdump.c
dfbdump_LDADD   = ../src/libdirectfb.la

fusion_bench_SOURCES = fusion_bench.c
fusion_bench_LDADD   = ../src/libdirectfb.la

raw15toraw24_SOURCES = raw15toraw24.c

raw16toraw24_SOURCES = raw16toraw24.c

raw32toraw24_SOURCES = raw32toraw24.c

EXTRA_DIST = \
	README			\
	dfb_static_build_example\
	gendoc.pl		\
	ipc_cleanup
