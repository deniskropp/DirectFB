## Makefile.am for DirectFB/docs

SUBDIRS = html

if DIRECTFB_CSOURCE
csource_mans = directfb-csource.1
endif

man_MANS = \
	$(csource_mans)	\
	dfbg.1		\
	directfbrc.5

HTMLMANS = $(patsubst %,%.html,$(man_MANS))


if HAVE_MAN2HTML

all-local: $(HTMLMANS)

clean-local:
	rm -f $(HTMLMANS)

%.html: %
	$(MAN2HTML) $* > $@

else

all-local:

clean-local:

endif


EXTRA_DIST = \
	README.screenshots	\
	dfbg.1.in		\
	directfbrc.5.in		\
	directfb-csource.1.in
