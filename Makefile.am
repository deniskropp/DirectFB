## Makefile.am for DirectFB

SUBDIRS = \
	patches 	\
	include 	\
	src 		\
	tools 		\
	gfxdrivers	\
	inputdrivers	\
	interfaces 	\
	examples 	\
	docs

bin_SCRIPTS = directfb-config

if AVIFILE_PROVIDER
pkgconfig_avifile = directfb-avifile.pc
endif

if FLASH_PROVIDER
pkgconfig_swf = directfb-swf.pc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = directfb.pc $(pkgconfig_avifile) $(pkgconfig_swf)


EXTRA_DIST = \
	README.screenshots	\
	directfb-avifile.pc	\
	directfb-swf.pc		\
	fb.modes
