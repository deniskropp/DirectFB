# Makefile.am for DirectFB-extra/interfaces/IDirectFBVideoProvider/video_out_dfb

video_out_dfbdir = $(XINE_PLUGINDIR)

INCLUDES = $(DFB_CFLAGS) $(XINE_CFLAGS)

if ENABLE_DEBUG
AM_CPPFLAGS = -DLOG=1 -DLOG_VERBOSE=1
endif

AM_CFLAGS = -fomit-frame-pointer


video_out_dfb_LTLIBRARIES = xineplug_vo_out_dfb.la

xineplug_vo_out_dfb_la_SOURCES = \
	video_out_dfb.c		\
	video_out_dfb.h		\
	video_out_dfb_proc.h	\
	video_out_dfb_mmxproc.h	\
	video_out_dfb_blend.h	

xineplug_vo_out_dfb_la_LIBADD = $(XINE_LIBS) $(DFB_LIBS)

xineplug_vo_out_dfb_la_LDFLAGS = -avoid-version -module -disable-static

