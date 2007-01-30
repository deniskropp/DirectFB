## Makefile.am for SaWMan/samples

INCLUDES    = \
	-I$(top_srcdir)/include	\
	$(DFB_CFLAGS)


noinst_PROGRAMS = sample1 testman testrun


sample1_SOURCES = sample1.c
sample1_LDADD   = $(DFB_LIBS) ../src/libsawman.la

testman_SOURCES = testman.c
testman_LDADD   = $(DFB_LIBS) ../src/libsawman.la

testrun_SOURCES = testrun.c
testrun_LDADD   = $(DFB_LIBS) ../src/libsawman.la
