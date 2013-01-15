## Makefile.am for examples/fusiondale

INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/lib/fusiondale \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib     \
	-I$(top_srcdir)/lib/fusiondale

LIBADDS = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/lib/fusiondale/libfusiondale.la

if ENABLE_VOODOO
VOODOO_TESTS = fd_client fd_proxy
else
VOODOO_TESTS =
endif

noinst_PROGRAMS = \
	fd_simple			\
	t2_sender		\
	t2_receiver		\
	data_test		\
	fdmaster		\
	ce_tune			\
	ce_tuner		\
	ce_tuning_monitor	\
	coma_test		\
	$(VOODOO_TESTS)

fd_simple_SOURCES = fd_simple.c
fd_simple_LDADD   = $(LIBADDS)

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

coma_test_SOURCES = coma_test.c
coma_test_LDADD   = $(LIBADDS)

fd_client_SOURCES = fd_client.c
fd_client_LDADD   = $(LIBADDS) $(top_builddir)/lib/voodoo/libvoodoo.la

fd_proxy_SOURCES = fd_proxy.c
fd_proxy_LDADD   = $(LIBADDS) $(top_builddir)/lib/voodoo/libvoodoo.la
