## Makefile.am for DirectFB/tools

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src


if DIRECTFB_CSOURCE
DFB_CSOURCE = directfb-csource

directfb_csource_SOURCES = directfb-csource.c
directfb_csource_LDADD   = $(LIBPNG)
endif

if HAVE_LINUX
LINUXONLY_TOOLS =	\
	dfbsummon	\
	fusion_bench	\
	raw15toraw24	\
	raw16toraw24	\
	raw32toraw24
endif

bin_PROGRAMS = $(DFB_CSOURCE) dfbdump dfbg dfbinfo dfblayer

noinst_PROGRAMS = \
	$(LINUXONLY_TOOLS)

dfbdump_SOURCES = dfbdump.c
dfbdump_LDADD   = ../src/libdirectfb.la

dfbg_SOURCES = dfbg.c
dfbg_LDADD   = ../src/libdirectfb.la

dfbinfo_SOURCES = dfbinfo.c
dfbinfo_LDADD   = ../src/libdirectfb.la

dfblayer_SOURCES = dfblayer.c
dfblayer_LDADD   = ../src/libdirectfb.la

dfbsummon_SOURCES = dfbsummon.c
dfbsummon_LDADD   = ../src/libdirectfb.la

fusion_bench_SOURCES = fusion_bench.c
fusion_bench_LDADD   = ../src/libdirectfb.la
fusion_bench_LDFLAGS = -udirectfb_fbdev

raw15toraw24_SOURCES = raw15toraw24.c

raw16toraw24_SOURCES = raw16toraw24.c

raw32toraw24_SOURCES = raw32toraw24.c

EXTRA_DIST = \
	README			\
	dfb_static_build_example\
	gendoc.pl
