## Makefile.am for DirectFB/tools

INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/lib/sawman \
	-I$(top_builddir)/lib/fusionsound \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/sawman	\
	-I$(top_srcdir)/lib/fusionsound	\
	-I$(top_srcdir)/src

AM_CPPFLAGS = -DBINDIR=\"@bindir@\" -DDATADIR=\"@DATADIR@\"

AM_CFLAGS = $(FREETYPE_CFLAGS) $(LIBPNG_CFLAGS)
AM_CXXFLAGS = $(LIBPNG_CFLAGS)


if BUILD_DIRECTFB_CSOURCE
DFB_CSOURCE = directfb-csource

directfb_csource_SOURCES = directfb-csource.c
directfb_csource_LDADD   = $(LIBPNG_LIBS)
endif

if HAVE_LINUX
LINUXONLY_TOOLS =	\
	fusion_bench	\
	raw15toraw24	\
	raw16toraw24	\
	raw32toraw24
endif

if ENABLE_VOODOO
VOODOO_PROGS = \
	dfbproxy			\
	voodooplay			\
	voodooplay_client		\
	voodooplay_server
endif

if ENABLE_SAWMAN
SAWMAN_PROGS = \
	swmdump
endif

if ENABLE_FUSIONSOUND
FUSIONSOUND_PROGS = \
	fsdump	\
	fsmaster	\
	fsplay	\
	fsvolume

if ENABLE_VOODOO
FUSIONSOUND_PROGS += fsproxy
endif
endif

if PNG_PROVIDER
PNG_PROGS = mkdfiff mkdgifft
endif

if FREETYPE_PROVIDER
FREETYPE_PROGS = mkdgiff
endif



if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_bin_PROGS = 
else
NON_PURE_VOODOO_bin_PROGS = \
	dfbdump			\
	dfbinput
endif

bin_PROGRAMS = \
	$(NON_PURE_VOODOO_bin_PROGS)	\
	$(DFB_CSOURCE)			\
	dfbfx				\
	dfbg				\
	dfbinfo				\
	dfbinspector			\
	dfblayer			\
	dfbmaster			\
	dfbscreen			\
	dfbpenmount			\
	$(PNG_PROGS)			\
	$(FREETYPE_PROGS)		\
	$(VOODOO_PROGS)			\
	$(SAWMAN_PROGS)			\
	$(FUSIONSOUND_PROGS)


if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_noinst_PROGS = 
else
NON_PURE_VOODOO_noinst_PROGS = \
	$(LINUXONLY_TOOLS)
endif

noinst_PROGRAMS = \
	$(NON_PURE_VOODOO_noinst_PROGS)



libdirectfb = ../src/libdirectfb.la
libfusion = ../lib/fusion/libfusion.la
libdirect = ../lib/direct/libdirect.la

if DIRECTFB_BUILD_ONE
libone = \
        ../lib/One/libone.la
else
libone =
endif

if DIRECTFB_BUILD_VOODOO
libvoodoo = \
        ../lib/voodoo/libvoodoo.la
else
libvoodoo =
endif

if DIRECTFB_BUILD_SAWMAN
libsawman = \
        ../lib/sawman/libsawman.la
else
libsawman =
endif

if ENABLE_FUSIONSOUND
libfusionsound = \
        ../lib/fusionsound/libfusionsound.la
else
libfusionsound =
endif

dfbdump_SOURCES = dfbdump.c
dfbdump_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect)

dfbg_SOURCES = dfbg.c
dfbg_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect) $(OSX_LIBS)

dfbinfo_SOURCES = dfbinfo.c
dfbinfo_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect) $(OSX_LIBS)

dfbinput_SOURCES = dfbinput.c
dfbinput_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect) $(OSX_LIBS)

dfbinspector_SOURCES = dfbinspector.c
dfbinspector_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect) $(OSX_LIBS)

dfblayer_SOURCES = dfblayer.c
dfblayer_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect) $(OSX_LIBS)

dfbmaster_SOURCES = dfbmaster.c
dfbmaster_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect) $(OSX_LIBS)

dfbproxy_SOURCES = dfbproxy.c
dfbproxy_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect) $(libvoodoo)

dfbscreen_SOURCES = dfbscreen.c
dfbscreen_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect) $(OSX_LIBS)

#dfbsummon_SOURCES = dfbsummon.c
#dfbsummon_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect)

dfbpenmount_SOURCES = dfbpenmount.c
dfbpenmount_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect)

mkdfiff_SOURCES = mkdfiff.c
mkdfiff_LDADD   = $(LIBPNG_LIBS) $(libdirect)

mkdgiff_SOURCES = mkdgiff.c
mkdgiff_LDADD   = $(FREETYPE_LIBS) $(libdirect)

mkdgifft_SOURCES = mkdgifft.cpp
mkdgifft_LDADD   = $(LIBPNG_LIBS) $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect)

fusion_bench_SOURCES = fusion_bench.c
fusion_bench_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect)

dfbfx_SOURCES = dfbfx.c
dfbfx_LDADD   = $(libdirect)

raw15toraw24_SOURCES = raw15toraw24.c

raw16toraw24_SOURCES = raw16toraw24.c

raw32toraw24_SOURCES = raw32toraw24.c

voodooplay_SOURCES = voodooplay.c
voodooplay_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect)

voodooplay_client_SOURCES = voodooplay_client.c
voodooplay_client_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect)

voodooplay_server_SOURCES = voodooplay_server.c
voodooplay_server_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect)

swmdump_SOURCES = swmdump.c
swmdump_LDADD   = $(libdirectfb) $(libvoodoo) $(libone) $(libfusion) $(libdirect) $(libsawman)

fsdump_SOURCES = fsdump.c
fsdump_LDADD   = $(libfusion) $(libdirect) $(libfusionsound)

fsmaster_SOURCES = fsmaster.c
fsmaster_LDADD   = $(libfusion) $(libdirect) $(libfusionsound)

fsplay_SOURCES = fsplay.c
fsplay_LDADD   = $(libfusion) $(libdirect) $(libfusionsound)

fsproxy_SOURCES = fsproxy.c
fsproxy_LDADD   = $(libfusion) $(libdirect) $(libfusionsound) $(libvoodoo)

fsvolume_SOURCES = fsvolume.c
fsvolume_LDADD   = $(libfusion) $(libdirect) $(libfusionsound)


EXTRA_DIST = \
	README			\
	gendoc.pl		\
	mknames.sh		\
	mkresult.sh
