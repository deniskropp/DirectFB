## Makefile.am for DirectFB

ACLOCAL_AMFLAGS = -I m4

if HAVE_LINUX
LINUXONLY_DIRS = inputdrivers gfxdrivers
endif

if BUILD_TOOLS
TOOLS_DIR = tools
endif

if BUILD_TESTS
TESTS_DIR = tests
endif

if ENABLE_VOODOO
PROXY_DIR = proxy
endif

SUBDIRS = \
	data			\
	docs			\
	include 		\
	lib			\
	src 			\
	systems			\
	$(TOOLS_DIR)		\
	wm			\
	interfaces 		\
	$(PROXY_DIR)		\
	$(LINUXONLY_DIRS)	\
	$(TESTS_DIR)		\
	patches			\
	rules

bin_SCRIPTS = directfb-config

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = directfb.pc directfb-internal.pc

EXTRA_DIST = \
	autogen.sh	\
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
