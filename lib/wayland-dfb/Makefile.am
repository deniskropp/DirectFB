pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = wayland-dfb.pc

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	$(WAYLAND_CFLAGS)

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"


includedir = @INCLUDEDIR@

include_HEADERS = wayland-dfb.h


lib_LTLIBRARIES = libwayland-dfb.la

libwayland_dfb_la_SOURCES = wayland-dfb.cpp wayland-dfb-protocol.c
libwayland_dfb_la_LDFLAGS = -version-info 1
libwayland_dfb_la_LIBADD = $(WAYLAND_LIBS)


BUILT_SOURCES = wayland-dfb-protocol.c \
		wayland-dfb-client-protocol.h \
		wayland-dfb-server-protocol.h

CLEANFILES = $(BUILT_SOURCES)

%-protocol.c : %.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) code < $< > $@

%-server-protocol.h : %.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) server-header < $< > $@

%-client-protocol.h : %.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < $< > $@

