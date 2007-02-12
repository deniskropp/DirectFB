## Makefile.am for DirectFB/tests

INCLUDES = \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-DLINUX_2_6


noinst_PROGRAMS = fusion_fork fusion_reactor fusion_stream

fusion_fork_SOURCES = fusion_fork.c
fusion_fork_LDADD   = ../src/libdirectfb.la ../lib/fusion/libfusion.la ../lib/direct/libdirect.la

fusion_reactor_SOURCES = fusion_reactor.c
fusion_reactor_LDADD   = ../src/libdirectfb.la ../lib/fusion/libfusion.la ../lib/direct/libdirect.la

fusion_stream_SOURCES = fusion_stream.c
fusion_stream_LDADD   = ../src/libdirectfb.la ../lib/fusion/libfusion.la ../lib/direct/libdirect.la
