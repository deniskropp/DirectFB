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

if GFX_SAVAGE
SAVAGE_DIR = savage
else
SAVAGE_DIR =
endif

SUBDIRS = $(ATI_DIR) $(MATROX_DIR) $(TDFX_DIR) $(NVIDIA_DIR) $(NEOMAGIC_DIR) $(SAVAGE_DIR)
