/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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




#ifndef __PVR2D_PVR2D_H__
#define __PVR2D_PVR2D_H__


#include <pvr2d.h>


#define GL_GLEXT_PROTOTYPES

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>


#define EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>


#include <fusion/shmalloc.h>

#include <core/surface_pool.h>


extern const SurfacePoolFuncs *pvr2dSurfacePoolFuncs;


typedef struct _NATIVE_PIXMAP_STRUCT
{
    long ePixelFormat;
    long eRotation;
    long lWidth;
    long lHeight;
    long lStride;
    long lSizeInBytes;
    long pvAddress;
    long lAddress;
}NATIVE_PIXMAP_STRUCT;


typedef struct {
     FusionSHMPoolShared *shmpool;

     CoreSurfacePool     *pool;

     DFBDimension         screen_size;
} PVR2DDataShared;

typedef struct {
     PVR2DDataShared     *shared;

     CoreDFB             *core;
     CoreScreen          *screen;
     CoreLayer           *layer;

     int                  nDevices;
     PVR2DDEVICEINFO     *pDevInfo;
     PVR2DCONTEXTHANDLE   hPVR2DContext;
     PVR2DMEMINFO        *pFBMemInfo;
     int                  nDeviceNum;
     PVR2DFORMAT          eDisplayFormat;
     long                 lStride;
     int                  RefreshRate;
     long                 lDisplayWidth;
     long                 lDisplayHeight;
     long                 lDisplayBitsPerPixel;

     NATIVE_PIXMAP_STRUCT nativePixmap;

     EGLConfig            eglConfig;
     EGLDisplay           eglDisplay;
     EGLSurface           eglSurface;
     EGLContext           eglContext;

     PFNEGLCREATEIMAGEKHRPROC                eglCreateImageKHR;
     PFNGLEGLIMAGETARGETTEXTURE2DOESPROC     glEGLImageTargetTexture2DOES;
} PVR2DData;

static inline bool TestEGLError(const char* pszLocation)
{
     EGLint iErr = eglGetError();
     if (iErr != EGL_SUCCESS) {
          D_ERROR("DirectFB/PVR2D: %s failed (%d).\n", pszLocation, iErr);
          return false;
     }

     return true;
}

#endif

