/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __EGL_EGL_H__
#define __EGL_EGL_H__


#include <EGL/egl.h>


#define GL_GLEXT_PROTOTYPES

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>


#define EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>


#include <fusion/shmalloc.h>

#include <core/surface_pool.h>


extern const SurfacePoolFuncs *eglSurfacePoolFuncs;


typedef struct {
     FusionSHMPoolShared *shmpool;

     CoreSurfacePool     *pool;

     DFBDimension         screen_size;
} EGLDataShared;

typedef struct {
     EGLDataShared     *shared;

     CoreDFB             *core;
     CoreScreen          *screen;
     CoreLayer           *layer;

     int                  nDevices;
     int                  nDeviceNum;
     long                 lStride;
     int                  RefreshRate;
     long                 lDisplayWidth;
     long                 lDisplayHeight;
     long                 lDisplayBitsPerPixel;


     EGLConfig            eglConfig;
     EGLDisplay           eglDisplay;
     EGLSurface           eglSurface;
     EGLContext           eglContext;

     PFNEGLCREATEIMAGEKHRPROC                eglCreateImageKHR;
     PFNGLEGLIMAGETARGETTEXTURE2DOESPROC     glEGLImageTargetTexture2DOES;
} EGLData;

static inline bool TestEGLError(const char* pszLocation)
{
     EGLint iErr = eglGetError();
     if (iErr != EGL_SUCCESS) {
          D_ERROR("DirectFB/EGL: %s failed (%d).\n", pszLocation, iErr);
          return false;
     }

     return true;
}

#endif

