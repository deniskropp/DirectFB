## Makefile.am for DirectFB/tests/voodoo

INCLUDES = \
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib

noinst_PROGRAMS = \
	voodoo_client	\
	voodoo_server

libvoodoo = $(top_builddir)/lib/voodoo/libvoodoo.la
libdirect = $(top_builddir)/lib/direct/libdirect.la

voodoo_client_SOURCES = voodoo_client.c
voodoo_client_LDADD   = $(libvoodoo) $(libdirect)

voodoo_server_SOURCES = voodoo_server.c
voodoo_server_LDADD   = $(libvoodoo) $(libdirect)

