## Makefile.am for DirectFB/inputdrivers

if LINUX_INPUT
LINUX_INPUT_DIR = linux_input
else
LINUX_INPUT_DIR =
endif

if H3600_TS
H3600_TS_DIR = h3600_ts
else
H3600_TS_DIR =
endif

if SONYPI
SONYPI_DIR = sonypi
else
SONYPI_DIR =
endif

if DBOX2REMOTE
DBOX2REMOTE_DIR = dbox2remote
else
DBOX2REMOTE_DIR =
endif

SUBDIRS = \
	joystick	\
	keyboard	\
	$(LINUX_INPUT_DIR) \
	lirc		\
	ps2mouse	\
	serialmouse	\
	$(H3600_TS_DIR) \
	$(SONYPI_DIR)	\
	$(DBOX2REMOTE_DIR)
