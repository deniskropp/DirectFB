include ../directfb-env.mk

# use this for apps using windows
CPP_FLAGS+= -DANDROID_USE_FBO_FOR_PRIMARY

DIRECTFB_APP_SOURCES := $(DFB_SOURCE)/../DirectFB-examples/src/df_window.c

include ../directfb.mk
