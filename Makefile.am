## Makefile.am for DirectFB

if HAVE_LINUX
LINUXONLY_DIRS =	\
	gfxdrivers	\
	inputdrivers
endif

if BUILD_TOOLS
TOOLS_DIR = tools
endif

SUBDIRS = \
	patches 		\
	include 		\
	src 			\
	$(TOOLS_DIR)		\
	$(LINUXONLY_DIRS)	\
	interfaces 		\
	data			\
	docs

bin_SCRIPTS = directfb-config

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = directfb.pc directfb-internal.pc

EXTRA_DIST = \
	README.screenshots	\
	fb.modes 		\
	directfb.spec.in	\
	directfb.spec


## Some special rules that may be useful ...

# Generate the HTML API documentation:
html:
	make -C docs/html

# Compile the directfb-csource utility:
directfb-csource:
	make -C tools directfb-csource


.PHONY: html directfb-csource
