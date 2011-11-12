ONE_CPPFLAGS := \
	-DONE_CORE_IP0=0x0A010117 \
	-DONE_CORE_IP1=0x0A01010D

EXTRA_DIST = Makefile.static

src/linux-one.ko: include/linux/one.h src/*.c src/*.h
	$(MAKE) -f $(srcdir)/Makefile.static "ONE_CPPFLAGS=$(ONE_CPPFLAGS) -I`readlink -f $(srcdir)`/.."

all-local:: src/linux-one.ko

clean-local::
	$(MAKE) -f $(srcdir)/Makefile.static clean
