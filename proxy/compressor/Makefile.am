## Makefile.am for FusionSound/proxy/compressor

INCLUDES = \
	-I$(top_srcdir)/include			\
	-I$(top_srcdir)/src 			\
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\" \
	$(VOODOO_CFLAGS)


noinst_LTLIBRARIES = libfusionsound_compressor.la

libfusionsound_compressor_la_SOURCES = dpack.c dpack.h dpack_proto.h
