## Makefile.am for DirectFB/interfaces/IDirectFBVideoProvider

idirectfbvideoproviderdir = $(MODULEDIR)/interfaces/IDirectFBVideoProvider


if AVIFILE_PROVIDER
AVIFILE_PROVIDER_LTLIB = libidirectfbvideoprovider_avifile.la
else
AVIFILE_PROVIDER_LTLIB =
endif


## This is a hack around fucking libtool to link libmpeg3.a into our shared object.

if LIBMPEG3_PROVIDER

clean-local:
	-rm -f libidirectfbvideoprovider_libmpeg3.so

all-local: libidirectfbvideoprovider_libmpeg3.so

libidirectfbvideoprovider_libmpeg3.so: idirectfbvideoprovider_libmpeg3.o $(LIBMPEG3_DIR)/$(shell uname --machine)/libmpeg3.a
	$(CC) -shared $+ -ldl -lpthread -lm -Wl,-soname -Wl,libidirectfbvideoprovider_libmpeg3.so -o $@

idirectfbvideoprovider_SCRIPTS = libidirectfbvideoprovider_libmpeg3.so

LIBMPEG3_INCLUDES = -I$(LIBMPEG3_DIR)

else

clean-local:
all-local:

endif

EXTRA_DIST = idirectfbvideoprovider_libmpeg3.c


if FLASH_PROVIDER
FLASH_PROVIDER_LTLIB = libidirectfbvideoprovider_swf.la
else
FLASH_PROVIDER_LTLIB =
endif


INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	-DDATADIR=\"@DATADIR@\"	\
	$(LIBMPEG3_INCLUDES)

CFLAGS = $(DFB_CFLAGS)
CXXFLAGS = $(DFB_CXXFLAGS) $(AVIFILE_CFLAGS)


idirectfbvideoprovider_LTLIBRARIES = \
	libidirectfbvideoprovider_v4l.la	\
	$(AVIFILE_PROVIDER_LTLIB)		\
	$(FLASH_PROVIDER_LTLIB)


libidirectfbvideoprovider_avifile_la_SOURCES = idirectfbvideoprovider_avifile.cpp

libidirectfbvideoprovider_avifile_la_LIBADD = $(AVIFILE_LIBS)

libidirectfbvideoprovider_avifile_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbvideoprovider_swf_la_SOURCES = idirectfbvideoprovider_swf.c

libidirectfbvideoprovider_swf_la_LIBADD = $(LIBFLASH)

libidirectfbvideoprovider_swf_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbvideoprovider_v4l_la_SOURCES = idirectfbvideoprovider_v4l.c

libidirectfbvideoprovider_v4l_la_LDFLAGS = -avoid-version -module -disable-static
