## Makefile.am for lib/fusionsound/core

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusionsound	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusionsound

AM_CPPFLAGS = \
     -DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"

internalincludedir = $(GENERIC_INCLUDEDIR)/fusionsound-internal/core

internalinclude_HEADERS = \
	sound_device.h \
	sound_driver.h \
	types_sound.h

noinst_LTLIBRARIES = \
	libfusionsoundcore.la


libfusionsoundcore_la_SOURCES = \
	core_sound.c		\
	core_sound.h		\
	playback.c		\
	playback.h		\
	playback_internal.h	\
	sound_buffer.c		\
	sound_buffer.h		\
	sound_device.c		\
	sound_device.h		\
	sound_driver.h		\
	sound_mix.h		\
	types_sound.h		\
	fs_types.h
