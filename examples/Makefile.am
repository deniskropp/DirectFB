## Makefile.am for FusionDale/examples

INCLUDES = -I$(top_srcdir)/include $(FUSION_CFLAGS)

LIBADDS = \
	$(top_builddir)/src/libfusiondale.la	\
	$(FUSION_LIBS)

bin_PROGRAMS    = fdmaster
noinst_PROGRAMS = simple t2_sender t2_receiver data_test

fdmaster_SOURCES = fdmaster.c
fdmaster_LDADD   = $(LIBADDS)

simple_SOURCES = simple.c
simple_LDADD   = $(LIBADDS)

t2_sender_SOURCES = t2_sender.c
t2_sender_LDADD   = $(LIBADDS)

t2_receiver_SOURCES = t2_receiver.c
t2_receiver_LDADD   = $(LIBADDS)

data_test_SOURCES = data_test.c
data_test_LDADD   = $(LIBADDS)
