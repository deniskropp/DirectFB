## Makefile.am for DirectFB/inputdrivers

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
	lirc		\
	ps2mouse	\
	serialmouse	\
	$(H3600_TS_DIR) \
	$(SONYPI_DIR)	\
	$(DBOX2REMOTE_DIR)
