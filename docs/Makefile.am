## Makefile.am for DirectFB/docs

SUBDIRS = html

if DIRECTFB_CSOURCE
csource_mans = directfb-csource.1
endif

man_MANS = \
	$(csource_mans)	\
	dfbg.1		\
	directfbrc.5

EXTRA_DIST = \
	README.screenshots	\
	dfbg.1.in		\
	directfbrc.5.in		\
	directfb-csource.1.in
