## Makefile.am for DirectFB-extras/interfaces/IDirectFBImageProvider/mpeg2

INCLUDES = \
    $(DFB_CFLAGS) \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

noinst_LTLIBRARIES = libmpeg2decode.la

libmpeg2decode_la_SOURCES = \
	getbits.c		\
	getblk.c		\
	gethdr.c		\
	getpic.c		\
	getvlc.c		\
	getvlc.h		\
	global.h		\
	idct.c			\
	idctref.c		\
	motion.c		\
	mpeg2dec.c		\
	mpeg2dec.h		\
	recon.c			\
	store.c			\
	systems.c

libmpeg2decode_la_LIBADD = -lm

libmpeg2decode_la_LDFLAGS = -avoid-version -module
