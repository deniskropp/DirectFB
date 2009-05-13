## Makefile.am for DirectFB/tools

INCLUDES = \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

AM_CPPFLAGS = -DDATADIR=\"@DATADIR@\"

AM_CFLAGS = $(FREETYPE_CFLAGS)

if BUILD_DIRECTFB_CSOURCE
DFB_CSOURCE = directfb-csource

directfb_csource_SOURCES = directfb-csource.c
directfb_csource_LDADD   = $(LIBPNG)
endif

if HAVE_LINUX
LINUXONLY_TOOLS =	\
	fusion_bench	\
	raw15toraw24	\
	raw16toraw24	\
	raw32toraw24
endif

if ENABLE_VOODOO
VOODOO_PROGS = dfbproxy
endif

if PNG_PROVIDER
PNG_PROGS = mkdfiff
endif

if FREETYPE_PROVIDER
FREETYPE_PROGS = mkdgiff
endif

bin_PROGRAMS = $(DFB_CSOURCE) \
	dfbdump dfbfx dfbg dfbinfo dfbinput dfbinspector dfblayer \
	dfbmaster dfbscreen dfbpenmount $(PNG_PROGS) $(FREETYPE_PROGS) $(VOODOO_PROGS)

noinst_PROGRAMS = $(LINUXONLY_TOOLS)

libdirectfb = ../src/libdirectfb.la
libfusion = ../lib/fusion/libfusion.la
libdirect = ../lib/direct/libdirect.la
libvoodoo = ../lib/voodoo/libvoodoo.la

dfbdump_SOURCES = dfbdump.c
dfbdump_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbg_SOURCES = dfbg.c
dfbg_LDADD   = $(libdirectfb) $(libfusion) $(libdirect) $(OSX_LIBS)

dfbinfo_SOURCES = dfbinfo.c
dfbinfo_LDADD   = $(libdirectfb) $(libfusion) $(libdirect) $(OSX_LIBS)

dfbinput_SOURCES = dfbinput.c
dfbinput_LDADD   = $(libdirectfb) $(libfusion) $(libdirect) $(OSX_LIBS)

dfbinspector_SOURCES = dfbinspector.c
dfbinspector_LDADD   = $(libdirectfb) $(libfusion) $(libdirect) $(OSX_LIBS)

dfblayer_SOURCES = dfblayer.c
dfblayer_LDADD   = $(libdirectfb) $(libfusion) $(libdirect) $(OSX_LIBS)

dfbmaster_SOURCES = dfbmaster.c
dfbmaster_LDADD   = $(libdirectfb) $(libfusion) $(libdirect) $(OSX_LIBS)

dfbproxy_SOURCES = dfbproxy.c
dfbproxy_LDADD   = $(libdirectfb) $(libfusion) $(libdirect) $(libvoodoo)

dfbscreen_SOURCES = dfbscreen.c
dfbscreen_LDADD   = $(libdirectfb) $(libfusion) $(libdirect) $(OSX_LIBS)

#dfbsummon_SOURCES = dfbsummon.c
#dfbsummon_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbpenmount_SOURCES = dfbpenmount.c
dfbpenmount_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

mkdfiff_SOURCES = mkdfiff.c
mkdfiff_LDADD   = $(LIBPNG) $(libdirect)

mkdgiff_SOURCES = mkdgiff.c
mkdgiff_LDADD   = $(FREETYPE_LIBS) $(libdirect)

fusion_bench_SOURCES = fusion_bench.c
fusion_bench_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbfx_SOURCES = dfbfx.c
dfbfx_LDADD   = $(libdirect)

raw15toraw24_SOURCES = raw15toraw24.c

raw16toraw24_SOURCES = raw16toraw24.c

raw32toraw24_SOURCES = raw32toraw24.c

EXTRA_DIST = \
	README			\
	gendoc.pl		\
	mknames.sh
