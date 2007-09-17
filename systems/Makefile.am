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

if VNC_CORE
VNC_DIR = vnc
else
VNC_DIR =
endif
  

SUBDIRS = $(DEVMEM_DIR) $(FBDEV_DIR) $(X11_DIR) $(SDL_DIR) $(OSX_DIR) $(VNC_DIR)
