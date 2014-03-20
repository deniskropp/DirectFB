## Makefile.am for DirectFB/lib/gles2


INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/lib/egl	\
	-I$(top_builddir)/src		\
	-I$(top_srcdir)/lib/egl		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/lib/egl		\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dfbgles2.pc gles2.pc


includedir = @INCLUDEDIR@/gles2

include_HEADERS = \
	dfbgles2.h		\
	dfbgles2_int_names.h	\
	gles2_result.h

DISTCLEANFILES = dfbgles2_int_names.h

GLES2_HEADER = $(top_srcdir)/lib/egl/GLES2/gl2.h $(top_srcdir)/lib/egl/GLES2/gl2ext.h $(top_srcdir)/lib/egl/GLES2/gl2dfbext.h

dfbgles2_int_names.h: $(GLES2_HEADER) Makefile
	@echo '#ifndef __DFBGLES2_INT_NAMES_H__' > $@
	@echo '#define __DFBGLES2_INT_NAMES_H__' >> $@
	@echo '' >> $@
	@echo '#include <GLES2/gl2.h>' >> $@
	@echo '#include <GLES2/gl2ext.h>' >> $@
	@echo '#include <GLES2/gl2dfbext.h>' >> $@

	@echo -e "\n\
	struct DFBGLES2IntName {\n\
	     GLint 	 value;\n\
	     const char *name;\n\
	};\n\
	\n\
	#define DirectFBGLES2IntNames(Identifier) struct DFBGLES2IntName Identifier[] = { \\" >> $@

	grep -hE '^#define[[:blank:]]+GL_[[:alnum:]_]+[[:blank:]]+(0x[[:alnum:]][[:alnum:]][[:alnum:]][[:alnum:]])\b' $(GLES2_HEADER) | perl -p -e 's/#define[ \t]+GL_([0-9A-Za-z_]+)[ \t]+(0x[0-9a-zA-Z][0-9a-zA-Z][0-9a-zA-Z][0-9a-zA-Z]).*/     \{ GL_\1 \/\*\2\*\/, \"\1\"\}, \\/' >> $@

	@echo '     { 0, "NULL" } \' >> $@
	@echo '};' >> $@
	@echo '' >> $@
	@echo '#endif' >> $@



lib_LTLIBRARIES = libDFBGLES2.la


libDFBGLES2_la_SOURCES = \
	api.cpp

libDFBGLES2_la_LIBADD = \
	$(DFB_BASE_LIBS) $(libdfbegl)

libDFBGLES2_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)                              	\
	$(DFB_LDFLAGS)



libgles2dir = $(libdir)/dfb-gles2

libgles2_LTLIBRARIES = libGLESv2.la

libGLESv2_la_SOURCES = \
	gles2_api.cpp

libGLESv2_la_LIBADD = \
	libDFBGLES2.la		\
	$(DFB_BASE_LIBS)

libGLESv2_la_LDFLAGS = \
	-version-number 1:0	\
	$(DFB_LDFLAGS)


LIBS_TO_NMFILE = libDFBGLES2.la


$(libDFBGLES2_la_SOURCES): dfbgles2_int_names.h


include $(top_srcdir)/rules/nmfile.make
include $(top_srcdir)/rules/libs_deps.make
