## Makefile.am for FusionDale/src/coma

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	$(FUSION_CFLAGS)

AM_CPPFLAGS = \
     -DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"

noinst_LTLIBRARIES = \
	libfusiondalecoma.la

comaincludedir = @INCLUDEDIR@/coma

comainclude_HEADERS = \
	coma.h			\
	coma_types.h		\
	component.h		\
	icoma.h			\
	icomacomponent.h

libfusiondalecoma_la_SOURCES = \
	coma.c			\
	component.c		\
	icoma.c			\
	icomacomponent.c
