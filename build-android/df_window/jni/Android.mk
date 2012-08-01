include ../directfb-env.mk

#CPPFLAGS+= -DANDROID_USE_FBO_FOR_PRIMARY
CPPFLAGS += -g

DIRECTFB_APP_SOURCES := $(DFB_SOURCE)/../DirectFB-examples/src/df_window.c

#DIRECTFB_APP_SOURCES := $(DFB_SOURCE)/../DirectFB-examples/src/jm_test.c

include ../directfb.mk
