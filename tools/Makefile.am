## Makefile.am for DirectFB/tools

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

if DIRECTFB_CSOURCE
bin_PROGRAMS = directfb-csource

directfb_csource_SOURCES = directfb-csource.c

directfb_csource_LDADD = $(LIBPNG)
endif

noinst_PROGRAMS = \
	raw15toraw24	\
	raw16toraw24	\
	raw32toraw24

raw15toraw24_SOURCES = raw15toraw24.c

raw16toraw24_SOURCES = raw16toraw24.c

raw32toraw24_SOURCES = raw32toraw24.c

EXTRA_DIST = \
	README			\
	dfb_static_build_example\
	gendoc.pl		\
	ipc_cleanup
