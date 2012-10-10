## Makefile.am for lib/fusiondale/coma

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusiondale	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusiondale

AM_CPPFLAGS = \
     -DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"

noinst_LTLIBRARIES = \
	libfusiondalecoma.la

comaincludedir = @GENERIC_INCLUDEDIR@/fusiondale/coma

comainclude_HEADERS = \
	coma.h			\
	coma_types.h		\
	component.h		\
	policy.h		\
	thread.h		\
	icoma.h			\
	icomacomponent.h

libfusiondalecoma_la_SOURCES = \
	coma.c			\
	component.c		\
	policy.c		\
	thread.c		\
	icoma.c			\
	icomacomponent.c
