## Makefile.am for DirectFB

if HAVE_LINUX
LINUXONLY_DIRS =	\
	gfxdrivers	\
	inputdrivers
else
LINUXONLY_DIRS =
endif

   
SUBDIRS = \
	patches 		\
	include 		\
	src 			\
	tools			\
	$(LINUXONLY_DIRS)	\
	interfaces 		\
	data			\
	docs

bin_SCRIPTS = directfb-config

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = directfb.pc directfb-internal.pc

EXTRA_DIST = \
	README.screenshots	\
	fb.modes

## Some special rules that may be useful ...
.PHONY: html directfb-csource

# Generate the HTML API documentation:
html:
	make -C docs/html

# Compile the directfb-csource utility:
directfb-csource:
	make -C tools directfb-csource
