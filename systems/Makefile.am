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

if FBDEV_CORE
FBDEV_DIR = fbdev
else
FBDEV_DIR =
endif

SUBDIRS = $(FBDEV_DIR) $(SDL_DIR) $(OSX_DIR)
