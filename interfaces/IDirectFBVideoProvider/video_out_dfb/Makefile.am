# Makefile.am for DirectFB-extra/interfaces/IDirectFBVideoProvider/video_out_dfb

video_out_dfbdir = $(XINE_PLUGINDIR)

INCLUDES = $(DFB_CFLAGS) $(XINE_CFLAGS)

AM_CFLAGS = -fomit-frame-pointer -ffast-math


video_out_dfb_LTLIBRARIES = xineplug_vo_out_dfb.la

xineplug_vo_out_dfb_la_SOURCES = video_out_dfb.c

xineplug_vo_out_dfb_la_LIBADD = $(XINE_LIBS) $(DFB_LIBS)

xineplug_vo_out_dfb_la_LDFLAGS = -avoid-version -module -disable-static


video_out_dfb.lo: $(srcdir)/video_out_dfb.c
	$(LIBTOOL) --mode=compile  $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) $(AM_CFLAGS) -prefer-non-pic \
	-c $(srcdir)/video_out_dfb.c
