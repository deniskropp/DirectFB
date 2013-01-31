## Makefile.am for DirectFB/systems

if SDL_CORE
SDL_DIR = sdl
else
SDL_DIR =
endif

if OSX_CORE
OSX_DIR = osx
else
OSX_DIR =
endif

if X11_CORE
X11_DIR = x11
else
X11_DIR =
endif

if X11VDPAU_CORE
X11VDPAU_DIR = x11vdpau
else
X11VDPAU_DIR =
endif

if PVR2D_CORE
PVR2D_DIR = pvr2d
else
PVR2D_DIR =
endif

if EGL_CORE
EGL_DIR = egl
else
EGL_DIR =
endif

if DEVMEM_CORE
DEVMEM_DIR = devmem
else
DEVMEM_DIR =
endif

if FBDEV_CORE
FBDEV_DIR = fbdev
else
FBDEV_DIR =
endif

if MESA_CORE
MESA_DIR = mesa
else
MESA_DIR =
endif

if DRMKMS_CORE
DRMKMS_DIR = drmkms
else
DRMKMS_DIR =
endif

if VNC_CORE
VNC_DIR = vnc
else
VNC_DIR =
endif
  

SUBDIRS = \
	android \
	dummy \
	$(PVR2D_DIR) \
	$(EGL_DIR) \
	$(DEVMEM_DIR) \
	$(FBDEV_DIR) \
	$(MESA_DIR) \
	$(DRMKMS_DIR) \
	$(X11_DIR) \
	$(X11VDPAU_DIR) \
	$(SDL_DIR) \
	$(OSX_DIR) \
	$(VNC_DIR)
