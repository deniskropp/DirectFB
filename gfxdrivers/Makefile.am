## Makefile.am for DirectFB/gfxdrivers

if GFX_ATI
ATI_DIR = ati128
else
ATI_DIR =
endif

if GFX_MATROX
MATROX_DIR = matrox
else
MATROX_DIR =
endif

if GFX_TDFX
TDFX_DIR = tdfx
else
TDFX_DIR =
endif

if GFX_NVIDIA
NVIDIA_DIR = nvidia
else
NVIDIA_DIR =
endif

if GFX_NEOMAGIC
NEOMAGIC_DIR = neomagic
else
NEOMAGIC_DIR =
endif

if GFX_NSC
GEODE_DIR = nsc
else
GEODE_DIR =
endif

if GFX_SAVAGE
SAVAGE_DIR = savage
else
SAVAGE_DIR =
endif

if GFX_CYBER5K
CYBER5K_DIR = cyber5k
else
CYBER5K_DIR =
endif

if GFX_I810
I810_DIR = i810
else
I810_DIR =
endif

SUBDIRS = \
	$(ATI_DIR)	\
	$(CYBER5K_DIR)	\
	$(MATROX_DIR)	\
	$(TDFX_DIR)	\
	$(NVIDIA_DIR)	\
	$(NEOMAGIC_DIR)	\
	$(SAVAGE_DIR)	\
	$(I810_DIR)	\
	$(GEODE_DIR)
