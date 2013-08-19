## Makefile.am for DirectFB/lib/egl


if ENABLE_WAYLAND
WAYLAND_DIRS = EGLCoreWayland
endif


SUBDIRS = . EGL GLES2 KHR $(WAYLAND_DIRS)


INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/src		\
	-I$(top_srcdir)/lib/egl		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dfbegl.pc egl.pc


includedir = @INCLUDEDIR@/egl

include_HEADERS = \
	dfbegl.h	\
	dfbegl_int_names.h

DISTCLEANFILES = dfbegl_int_names.h

EGL_HEADER = $(srcdir)/EGL/egl.h $(srcdir)/EGL/eglext.h $(srcdir)/EGL/egldfbext.h

dfbegl_int_names.h: $(EGL_HEADER) Makefile
	@echo '#ifndef __DFBEGL_INT_NAMES_H__' > $@
	@echo '#define __DFBEGL_INT_NAMES_H__' >> $@
	@echo '' >> $@
	@echo '#include <EGL/egl.h>' >> $@
	@echo '#include <EGL/eglext.h>' >> $@
	@echo '#include <EGL/egldfbext.h>' >> $@

	@echo -e "\n\
	struct DFBEGLIntName {\n\
	     EGLint 	 value;\n\
	     const char *name;\n\
	};\n\
	\n\
	#define DirectFBEGLIntNames(Identifier) struct DFBEGLIntName Identifier[] = { \\" >> $@

	grep -hE '^#define[[:blank:]]+EGL_[[:alnum:]_]+[[:blank:]]+(0x[[:alnum:]][[:alnum:]][[:alnum:]][[:alnum:]])\b' $(EGL_HEADER) | perl -p -e 's/#define[ \t]+EGL_([0-9A-Za-z_]+)[ \t]+(0x[0-9a-zA-Z][0-9a-zA-Z][0-9a-zA-Z][0-9a-zA-Z]).*/     \{ EGL_\1 \/\*\2\*\/, \"\1\"\}, \\/' >> $@

	@echo '     { 0, "NULL" } \' >> $@
	@echo '};' >> $@
	@echo '' >> $@
	@echo '#endif' >> $@



lib_LTLIBRARIES = libDFBEGL.la


libDFBEGL_la_SOURCES = \
	api.cpp			\
	config.cpp		\
	context.cpp		\
	core.cpp		\
	display.cpp		\
	library.cpp		\
	image.cpp		\
	option.cpp		\
	surface.cpp		\
	tls.cpp			\
	util.cpp

libDFBEGL_la_LIBADD = \
	$(DFB_BASE_LIBS)

libDFBEGL_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)                              	\
	$(DFB_LDFLAGS)



libegldir = $(libdir)/dfb-egl

libegl_LTLIBRARIES = libEGL.la

libEGL_la_SOURCES = \
	egl_api.cpp

libEGL_la_LIBADD = \
	libDFBEGL.la		\
	$(libdfbegl)		\
	$(DFB_BASE_LIBS)

libEGL_la_LDFLAGS = \
	-version-number 1:0	\
	$(DFB_LDFLAGS)


LIBS_TO_NMFILE = libDFBEGL.la


#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libDFBEGL_fixed.a

all-local: libDFBEGL_fixed.a

libDFBEGL_fixed.a: .libs/libDFBEGL.a
	rm -f libDFBEGL_fixed.a
	${AR} cru libDFBEGL_fixed.a `find . -name "*.o" | grep -v '.libs'`
	${RANLIB} libDFBEGL_fixed.a
	cp -pf libDFBEGL_fixed.a .libs/libDFBEGL.a

.libs/libDFBEGL.a: libDFBEGL.la

else

clean-local:

all-local:

endif



$(libDFBEGL_la_SOURCES): dfbegl_int_names.h


include $(top_srcdir)/rules/nmfile.make
include $(top_srcdir)/rules/libs_deps.make
