## Makefile.am for DirectFB/inputdrivers

if ELO_INPUT
ELO_INPUT_DIR = elo
endif

if LINUX_INPUT
LINUX_INPUT_DIR = linux_input
endif

if H3600_TS
H3600_TS_DIR = h3600_ts
endif

if UCB1X00_TS
UCB1X00_TS_DIR = ucb1x00_ts
endif

if SONYPI
SONYPI_DIR = sonypi
endif

if DBOX2REMOTE
DBOX2REMOTE_DIR = dbox2remote
endif

if MUTOUCH_TS
MUTOUCH_TS_DIR = mutouch
endif

if WM97XX_TS
WM97XX_TS_DIR = wm97xx_ts
endif

SUBDIRS = \
	$(ELO_INPUT_DIR)	\
	joystick		\
	keyboard		\
	$(LINUX_INPUT_DIR)	\
	lirc			\
	ps2mouse		\
	serialmouse		\
	$(H3600_TS_DIR)		\
	$(UCB1X00_TS_DIR)	\
	$(MUTOUCH_TS_DIR)	\
	$(SONYPI_DIR)		\
	$(DBOX2REMOTE_DIR)	\
	$(WM97XX_TS_DIR)
