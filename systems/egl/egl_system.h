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
#define EGL_EGLEXT_PROTOTYPES

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>



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
     int                  Stride;
     int                  RefreshRate;
     int                  DisplayWidth;
     int                  DisplayHeight;
     int                  DisplayBitsPerPixel;


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

EGLAPI void EGLAPIENTRY eglCreateGlobalImageBRCM(EGLint width, EGLint height, EGLint pixel_format, const void *data, EGLint data_stride, EGLint *id);
#define EGL_NATIVE_PIXMAP_CLIENT_SIDE_BRCM 0x99930B0 
#define EGL_IMAGE_WRAP_BRCM 0x9993140
#define EGL_IMAGE_WRAP_BRCM_BCG 0x9993141

#define EGL_PIXEL_FORMAT_XRGB_8888_BRCM     2
#define EGL_PIXEL_FORMAT_ARGB_8888_BRCM     1

typedef enum
{
   /* These formats are render target formats, but cannot be textured from */
   BEGL_BufferFormat_eA8B8G8R8,
   BEGL_BufferFormat_eR8G8B8A8,
   BEGL_BufferFormat_eX8B8G8R8,
   BEGL_BufferFormat_eR8G8B8X8,
   BEGL_BufferFormat_eR5G6B5, 
   /* These formats can be textured from, but cannot be rendered into */
   BEGL_BufferFormat_eYUV422_Texture,
   BEGL_BufferFormat_eA8B8G8R8_Texture,
   /* These formats are TFormat variants */
   BEGL_BufferFormat_eR8G8B8A8_TFormat,
   BEGL_BufferFormat_eX8G8B8A8_TFormat,
   BEGL_BufferFormat_eR5G6B5_TFormat,
   BEGL_BufferFormat_eR5G5B5A1_TFormat,
   BEGL_BufferFormat_eR4G4B4A4_TFormat,
   /* These are the LT versions for small textures */
   BEGL_BufferFormat_eR8G8B8A8_LTFormat,
   BEGL_BufferFormat_eX8G8B8A8_LTFormat,
   BEGL_BufferFormat_eR5G6B5_LTFormat,
   BEGL_BufferFormat_eR5G5B5A1_LTFormat,
   BEGL_BufferFormat_eR4G4B4A4_LTFormat,
   /* Can be used to return back an invalid format */
   BEGL_BufferFormat_INVALID   
} BEGL_BufferFormat;


typedef struct {
   BEGL_BufferFormat format;

   uint16_t width;
   uint16_t height;

   int32_t stride; /* in bytes */

   void *storage;
} EGL_IMAGE_WRAP_BRCM_BC_IMAGE_T;  

#endif

