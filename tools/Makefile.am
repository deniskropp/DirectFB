## Makefile.am for DirectFB/tools

INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/lib/sawman \
	-I$(top_builddir)/lib/divine \
	-I$(top_builddir)/lib/fusiondale \
	-I$(top_builddir)/lib/fusionsound \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/divine	\
	-I$(top_srcdir)/lib/sawman	\
	-I$(top_srcdir)/lib/fusiondale	\
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

if ENABLE_FUSIONDALE
FUSIONDALE_PROGS = \
	fddump \
	fdmaster
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

if ENABLE_DIVINE
DIVINE_PROGS = \
	spooky
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
	dfbdumpinput		\
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
	$(FUSIONDALE_PROGS)		\
	$(FUSIONSOUND_PROGS)		\
	$(DIVINE_PROGS)

if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_noinst_PROGS = 
else
NON_PURE_VOODOO_noinst_PROGS = \
	$(LINUXONLY_TOOLS)
endif

noinst_PROGRAMS = \
	$(NON_PURE_VOODOO_noinst_PROGS)


# for DFB_BASE_LIBS etc...
include $(top_srcdir)/rules/libs_deps.make


dfbdump_SOURCES = dfbdump.c
dfbdump_LDADD   = $(DFB_BASE_LIBS)

dfbg_SOURCES = dfbg.c
dfbg_LDADD   = $(DFB_BASE_LIBS) $(OSX_LIBS)

dfbinfo_SOURCES = dfbinfo.c
dfbinfo_LDADD   = $(DFB_BASE_LIBS) $(OSX_LIBS)

dfbdumpinput_SOURCES = dfbdumpinput.c
dfbdumpinput_LDADD   = $(DFB_BASE_LIBS) $(OSX_LIBS)

dfbinput_SOURCES = dfbinput.c
dfbinput_LDADD   = $(DFB_BASE_LIBS) $(OSX_LIBS)

dfbinspector_SOURCES = dfbinspector.c
dfbinspector_LDADD   = $(DFB_BASE_LIBS) $(OSX_LIBS)

dfblayer_SOURCES = dfblayer.c
dfblayer_LDADD   = $(DFB_BASE_LIBS) $(OSX_LIBS)

dfbmaster_SOURCES = dfbmaster.c
dfbmaster_LDADD   = $(DFB_BASE_LIBS) $(OSX_LIBS)

dfbproxy_SOURCES = dfbproxy.c
dfbproxy_LDADD   = $(DFB_BASE_LIBS)

dfbscreen_SOURCES = dfbscreen.c
dfbscreen_LDADD   = $(DFB_BASE_LIBS) $(OSX_LIBS)

#dfbsummon_SOURCES = dfbsummon.c
#dfbsummon_LDADD   = $(DFB_BASE_LIBS)

dfbpenmount_SOURCES = dfbpenmount.c
dfbpenmount_LDADD   = $(DFB_BASE_LIBS)

mkdfiff_SOURCES = mkdfiff.c
mkdfiff_LDADD   = $(LIBPNG_LIBS) $(libdirect)

mkdgiff_SOURCES = mkdgiff.c
mkdgiff_LDADD   = $(FREETYPE_LIBS) $(libdirect)

mkdgifft_SOURCES = mkdgifft.cpp
mkdgifft_LDADD   = $(LIBPNG_LIBS) $(DFB_BASE_LIBS)

fusion_bench_SOURCES = fusion_bench.c
fusion_bench_LDADD   = $(DFB_BASE_LIBS)

dfbfx_SOURCES = dfbfx.c
dfbfx_LDADD   = $(libdirect)

raw15toraw24_SOURCES = raw15toraw24.c

raw16toraw24_SOURCES = raw16toraw24.c

raw32toraw24_SOURCES = raw32toraw24.c

voodooplay_SOURCES = voodooplay.c
voodooplay_LDADD   = $(DFB_BASE_LIBS)

voodooplay_client_SOURCES = voodooplay_client.c
voodooplay_client_LDADD   = $(DFB_BASE_LIBS)

voodooplay_server_SOURCES = voodooplay_server.c
voodooplay_server_LDADD   = $(DFB_BASE_LIBS)

swmdump_SOURCES = swmdump.c
swmdump_LDADD   = $(DFB_BASE_LIBS) $(libsawman)

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

spooky_SOURCES = spooky.c
spooky_LDADD   = $(DFB_BASE_LIBS) $(libdivine) -lm

fddump_SOURCES = fddump.c
fddump_LDADD   = $(libfusion) $(libdirect) $(libfusiondale)

fdmaster_SOURCES = fdmaster.c
fdmaster_LDADD   = $(libfusion) $(libdirect) $(libfusiondale)


EXTRA_DIST = \
	README			\
	gendoc.pl		\
	mknames.sh		\
	scroll_circle.spooky	\
	scroll_diagonal.spooky	\
	scroll_vertical.spooky	\
	mkresult.sh
