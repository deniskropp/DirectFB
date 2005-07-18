## Makefile.am for DirectFB/wm/unique

SUBDIRS = classes data devices

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/wm		\
	-I$(top_builddir)/wm

bin_PROGRAMS = uwmdump

noinst_PROGRAMS = test_color test_foo stret_test


uwmdump_SOURCES = uwmdump.c

uwmdump_LDADD = \
	libuniquewm.la					\
	$(top_builddir)/lib/direct/libdirect.la		\
	$(top_builddir)/lib/fusion/libfusion.la		\
	$(top_builddir)/src/libdirectfb.la


test_color_SOURCES = test_color.c

test_color_LDADD = \
	libuniquewm.la					\
	$(top_builddir)/lib/direct/libdirect.la		\
	$(top_builddir)/lib/fusion/libfusion.la		\
	$(top_builddir)/src/libdirectfb.la


test_foo_SOURCES = test_foo.c

test_foo_LDADD = \
	libuniquewm.la					\
	$(top_builddir)/lib/direct/libdirect.la		\
	$(top_builddir)/lib/fusion/libfusion.la		\
	$(top_builddir)/src/libdirectfb.la


stret_test_SOURCES = stret_test.c

stret_test_LDADD = \
	libuniquewm.la					\
	$(top_builddir)/lib/fusion/libfusion.la		\
	$(top_builddir)/lib/direct/libdirect.la		\
	$(top_builddir)/src/libdirectfb.la


# Set module installation directory
wmdir = $(MODULEDIR)/wm

# Build wm module
wm_LTLIBRARIES = libdirectfbwm_unique.la

if BUILD_STATIC
wm_DATA = libdirectfbwm_unique.o
endif

libdirectfbwm_unique_la_SOURCES = \
	unique.c

# Link module against uniquewm library
libdirectfbwm_unique_la_LIBADD = libuniquewm.la

libdirectfbwm_unique_la_LDFLAGS = \
	-avoid-version	\
	-module

# Set header installation directory
includedir = @INCLUDEDIR@/unique

# Install uniquewm's library header files
include_HEADERS = \
	context.h		\
	decoration.h		\
	device.h		\
	input_channel.h		\
	input_events.h		\
	input_switch.h		\
	stret.h			\
	stret_iteration.h	\
	types.h			\
	uniquewm.h		\
	window.h

# Build uniquewm library
lib_LTLIBRARIES = libuniquewm.la

libuniquewm_la_SOURCES = \
	context.c		\
	decoration.c		\
	device.c		\
	input_channel.c		\
	input_switch.c		\
	internal.h		\
	stret.c			\
	stret_iteration.c	\
	uniquewm.c		\
	window.c

libuniquewm_la_LIBADD = \
	classes/libunique_classes.la			\
	devices/libunique_devices.la			\
	$(top_builddir)/lib/direct/libdirect.la		\
	$(top_builddir)/lib/fusion/libfusion.la		\
	$(top_builddir)/src/libdirectfb.la

libuniquewm_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)					\
	$(DFB_LDFLAGS)


include $(top_srcdir)/rules/libobject.make

#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libuniquewm_fixed.a

all-local: libuniquewm_fixed.a

libuniquewm_fixed.a: .libs/libuniquewm.a
	rm -f libuniquewm_fixed.a
	${AR} cru libuniquewm_fixed.a `find . -name "*.o" | grep -v '.libs'`
	${RANLIB} libuniquewm_fixed.a
	cp -af libuniquewm_fixed.a .libs/libuniquewm.a

.libs/libuniquewm.a: libuniquewm.la

else

clean-local:

all-local:

endif
