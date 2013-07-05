INCLUDES = \
	-I$(top_srcdir)/include	\
	@DIRECTFB_CFLAGS@ -Wall

lib_LTLIBRARIES = lib++dfb.la

lib__dfb_la_SOURCES = \
	ppdfb.cpp			\
	++dfb_internal.h		\
	idirectfb.cpp			\
	idirectfbdatabuffer.cpp		\
	idirectfbdisplaylayer.cpp	\
	idirectfbeventbuffer.cpp	\
	idirectfbfont.cpp		\
	idirectfbimageprovider.cpp	\
	idirectfbinputdevice.cpp	\
	idirectfbpalette.cpp		\
	idirectfbscreen.cpp		\
	idirectfbsurface.cpp		\
	idirectfbvideoprovider.cpp	\
	idirectfbwindow.cpp

lib__dfb_la_LIBADD = @DIRECTFB_LIBS@

lib__dfb_la_LDFLAGS = \
        -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
        -release $(LT_RELEASE)
