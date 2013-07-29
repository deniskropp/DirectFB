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

if BUILD_DVC
DVC_DIR = lib/dvc
endif

if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_SUBDIRS = 
else
NON_PURE_VOODOO_SUBDIRS = \
	lib/egl			\
	systems			\
	wm			\
	interfaces 		\
	$(LINUXONLY_DIRS)
endif

if ENABLE_DIVINE
DIVINE_DIR = lib/divine
endif

if ENABLE_SAWMAN
SAWMAN_DIR = lib/sawman
endif

if ENABLE_FUSIONDALE
FUSIONDALE_EXAMPLES_DIR = examples/fusiondale
endif

if ENABLE_FUSIONSOUND
FUSIONSOUND_EXAMPLES_DIR = examples/fusionsound
endif

if ENABLE_WAYLAND
WAYLAND_DIRS = lib/wayland-dfb
endif

SUBDIRS = \
	data			\
	docs			\
	include 		\
	include/++dfb		\
	lib			\
	src 			\
	lib/++dfb		\
	$(WAYLAND_DIRS)		\
	$(SAWMAN_DIR)		\
	$(DIVINE_DIR)		\
	$(DVC_DIR)		\
	$(TOOLS_DIR)		\
	$(NON_PURE_VOODOO_SUBDIRS)	\
	$(PROXY_DIR)		\
	$(TESTS_DIR)		\
	$(FUSIONDALE_EXAMPLES_DIR) \
	$(FUSIONSOUND_EXAMPLES_DIR) \
	examples/++dfb		\
	patches			\
	rules			\
	build-android


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



# shortcuts
lib_direct:
	$(MAKE) -C lib/direct

lib_fusion: lib_direct
	$(MAKE) -C lib/fusion

lib_fusiondale: lib_fusion lib_direct
	$(MAKE) -C lib/fusiondale

lib_fusionsound: lib_fusion lib_direct
	$(MAKE) -C lib/fusionsound

lib_one: lib_direct
	$(MAKE) -C lib/One

lib_voodoo: lib_direct
	$(MAKE) -C lib/voodoo



src_core:
	$(MAKE) -C src/core

src_display:
	$(MAKE) -C src/display

src_gfx:
	$(MAKE) -C src/gfx

src_input:
	$(MAKE) -C src/input

src_media:
	$(MAKE) -C src/media

src_misc:
	$(MAKE) -C src/misc

src_windows:
	$(MAKE) -C src/windows

src_: src_windows src_misc src_media src_input src_gfx src_display src_core   lib_voodoo lib_one lib_fusion lib_direct
	$(MAKE) -C src



lib_divine: src_
	$(MAKE) -C lib/divine

lib_egl: lib_wayland_dfb src_
	$(MAKE) -C lib/egl

lib_ppdfb: src_
	$(MAKE) -C lib/++dfb

lib_sawman: src_
	$(MAKE) -C lib/sawman

lib_wayland_dfb: src_
	$(MAKE) -C lib/wayland-dfb

lib_wayland_egl: lib_direct
	$(MAKE) -C lib/wayland-egl



gfxdrivers_gles2: src_
	$(MAKE) -C gfxdrivers/gles2

systems_x11: lib_egl src_
	$(MAKE) -C systems/x11

tests_: lib_egl lib_divine lib_ppdfb src_
	$(MAKE) -C tests

tools_: lib_fusionsound lib_fusiondale lib_egl lib_divine src_
	$(MAKE) -C tools

wm_default: src_
	$(MAKE) -C wm/default

wm_sawman: src_ lib_sawman
	$(MAKE) -C wm/sawman



fast_dfb: wm_default wm_sawman tools_ tests_ systems_x11 gfxdrivers_gles2

fast: fast_dfb
	$(MAKE)

fast-install: fast_dfb
	for dir in \
	            lib/direct               \
	            lib/fusion  	     \
	            lib/fusiondale           \
	            lib/fusionsound          \
	            lib/One     	     \
	            lib/voodoo  	     \
	            src/core    	     \
	            src/display 	     \
	            src/gfx     	     \
	            src/input   	     \
	            src/media   	     \
	            src/misc    	     \
	            src/windows 	     \
	            src 		     \
	            lib/divine  	     \
	            lib/egl     	     \
	            lib/++dfb   	     \
	            lib/sawman  	     \
	            lib/wayland-dfb          \
	            lib/wayland-egl          \
	            gfxdrivers/gles2         \
	            systems/x11 	     \
	            tests       	     \
	            tools       	     \
	            wm/default  	     \
	            wm/sawman   	     \
	; do $(MAKE) -C $$dir install; done

