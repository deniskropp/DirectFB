## Makefile.am for DirectFB/lib/voodoo

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"


# If the old location isn't cleared, builds of external modules fail
install-exec-local:
	rm -rf $(INTERNALINCLUDEDIR)/voodoo


includedir = @INCLUDEDIR@/voodoo

include_HEADERS = \
	build.h			\
	client.h		\
	conf.h			\
	interface.h		\
	manager.h		\
	message.h		\
	server.h


lib_LTLIBRARIES = libvoodoo.la

libvoodoo_la_SOURCES = \
	client.c		\
	conf.c			\
	interface.c		\
	internal.h		\
	manager.c		\
	server.c

libvoodoo_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)					\
	$(DFB_LDFLAGS)

libvoodoo_la_LIBADD = ../direct/libdirect.la


#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libvoodoo_fixed.a

all-local: libvoodoo_fixed.a

libvoodoo_fixed.a: .libs/libvoodoo.a
	rm -f libvoodoo_fixed.a
	${AR} cru libvoodoo_fixed.a `find -name "*.o"`
	${RANLIB} libvoodoo_fixed.a
	cp -af libvoodoo_fixed.a .libs/libvoodoo.a

.libs/libvoodoo.a: libvoodoo.la

else

clean-local:

all-local:

endif
