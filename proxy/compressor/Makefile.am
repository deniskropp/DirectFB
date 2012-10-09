## Makefile.am for FusionSound/proxy/compressor

INCLUDES = \
	-I$(top_builddir)/include			\
	-I$(top_builddir)/src 			\
	-I$(top_builddir)/lib 			\
	-I$(top_builddir)/lib/fusionsound	\
	-I$(top_srcdir)/include			\
	-I$(top_srcdir)/src 			\
	-I$(top_srcdir)/lib 			\
	-I$(top_srcdir)/lib/fusionsound		\
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"


noinst_LTLIBRARIES = libfusionsound_compressor.la

libfusionsound_compressor_la_SOURCES = dpack.c dpack.h dpack_proto.h
