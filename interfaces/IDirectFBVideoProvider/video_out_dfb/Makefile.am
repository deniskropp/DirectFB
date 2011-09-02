# Makefile.am for DirectFB/interfaces/IDirectFBVideoProvider/video_out_dfb

video_out_dfbdir = $(XINE_PLUGINDIR)

INCLUDES = -I$(top_builddir)/include \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
        $(XINE_CFLAGS)

if ENABLE_DEBUG
AM_CPPFLAGS = -DLOG=1
endif

video_out_dfb_LTLIBRARIES = xineplug_vo_out_dfb.la

xineplug_vo_out_dfb_la_SOURCES = \
	video_out_dfb.c 		\
	video_out_dfb.h 		\
	video_out_dfb_mix.h 	\
	video_out_dfb_blend.h	

xineplug_vo_out_dfb_la_LIBADD = $(XINE_LIBS) $(DFB_LIBS)

xineplug_vo_out_dfb_la_LDFLAGS = -avoid-version -module -disable-static

