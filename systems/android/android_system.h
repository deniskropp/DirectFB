/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __ANDROID_ANDROID_H__
#define __ANDROID_ANDROID_H__


#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "android-dfb", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "android-dfb", __VA_ARGS__))


#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef GL_OES_EGL_image
static PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES_func;
#endif

#include <fusion/shmalloc.h>

#include <core/surface_pool.h>

#include <core/layers.h>
#include <core/screens.h>


extern const SurfacePoolFuncs   androidSurfacePoolFuncs;

extern const ScreenFuncs       *androidScreenFuncs;
extern const DisplayLayerFuncs *androidLayerFuncs;


typedef struct {
     FusionSHMPoolShared *shmpool;

     CoreSurfacePool     *pool;

     DFBDimension         screen_size;
} AndroidDataShared;

typedef struct {
     AndroidDataShared   *shared;

     CoreDFB             *core;
     CoreScreen          *screen;
     CoreLayer           *layer;

     EGLDisplay           dpy;
     EGLContext           ctx;
     EGLSurface           surface;
} AndroidData;



/**
 * Shared state for our app.
 */
typedef struct {
    struct android_app* app;

    ASensorManager* sensorManager;
    const ASensor* accelerometerSensor;
    ASensorEventQueue* sensorEventQueue;

    DirectThread *main_thread;
} AndroidNativeData;

#endif

