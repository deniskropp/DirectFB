## Makefile.am for FusionDale/examples

INCLUDES = \
	-I$(top_srcdir)/include			\
	$(FUSION_CFLAGS)			\
	$(VOODOO_CFLAGS)

LIBADDS = \
	$(top_builddir)/src/libfusiondale.la	\
	$(FUSION_LIBS)				\
	$(VOODOO_LIBS)

if HAVE_VOODOO
VOODOO_TESTS = fd_client fd_proxy
else
VOODOO_TESTS =
endif

noinst_PROGRAMS = \
	simple			\
	t2_sender		\
	t2_receiver		\
	data_test		\
	fdmaster		\
	ce_tune			\
	ce_tuner		\
	ce_tuning_monitor	\
	$(VOODOO_TESTS)

simple_SOURCES = simple.c
simple_LDADD   = $(LIBADDS)

t2_sender_SOURCES = t2_sender.c
t2_sender_LDADD   = $(LIBADDS)

t2_receiver_SOURCES = t2_receiver.c
t2_receiver_LDADD   = $(LIBADDS)

data_test_SOURCES = data_test.c
data_test_LDADD   = $(LIBADDS)

fdmaster_SOURCES = fdmaster.c
fdmaster_LDADD   = $(LIBADDS)

ce_tune_SOURCES = ce_tune.c
ce_tune_LDADD   = $(LIBADDS)

ce_tuner_SOURCES = ce_tuner.c ce_tuner.h
ce_tuner_LDADD   = $(LIBADDS)

ce_tuning_monitor_SOURCES = ce_tuning_monitor.c
ce_tuning_monitor_LDADD   = $(LIBADDS)

fd_client_SOURCES = fd_client.c
fd_client_LDADD   = $(LIBADDS) $(VOODOO_LIBS)

fd_proxy_SOURCES = fd_proxy.c
fd_proxy_LDADD   = $(LIBADDS) $(VOODOO_LIBS)
