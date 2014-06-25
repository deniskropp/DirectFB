/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef ___DFBEGL__dfbegl__H___
#define ___DFBEGL__dfbegl__H___


#ifdef DFBEGL_ENABLE_MANGLE
#ifdef __egl_h_
#error Must include dfbegl.h before egl.h!
#endif

#include "EGL/egldfbmangle.h"
#endif


#define EGL_EGLEXT_PROTOTYPES


#include <directfb.h>    // MUST INCLUDE BEFORE <EGL/egl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>



#ifdef __cplusplus
extern "C" {
#endif


// C wrappers

//char *ToString_EGLInts( const EGLint *ints, EGLint num );


#ifdef __cplusplus
}

#include <direct/Map.h>
#include <direct/String.h>
#include <direct/TLSObject.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>


#include <list>
#include <map>



/*

  EGL (Core)


  = Display Modules
    - Native/X11 (X11 system module)
    - Native/XDirectFB (app module)
    - Native/Wayland
    - Native/SDL

    = Extensions
      - WL/BindDisplay

  = Implementations (see more in core/Graphics.h)
    - DRM/EGL   (DRM system module + Mesa egl library)
    - X11/EGL   (X11 system module + Mesa egl library)
    - DirectVG  (Independent module)


  = Surfaces

     Functions

          eglCreatePixmapSurface( *config*, handle, attr )
          eglCreateWindowSurface( *config*, handle, attr )

          eglCreatePbufferSurface( *config*, attr )


     Native Handles
                         DFB                 X11                 DRM       Wayland
               PIXMAP    IDirectFBSurface    Pixmap (XID)        bo        ?
               WINDOW    IDirectFBSurface    Window (XID)        bo        wl_egl_window



  = Client Buffers

     Functions

          eglCreateImage( *context*, target, buffer, attr )

          eglCreatePbufferFromClientBuffer( *config*, buftype, buffer, attr )


     Types

          target                                                 buffer type

          - Core Extensions
 
             - EGL_IDIRECTFBSURFACE_DIRECTFB

             - EGL_WAYLAND_BUFFER_WL
             - EGL_WAYLAND_PLANE_WL

             - EGL_DRM_BUFFER_MESA

             - EGL_FRAMEBUFFER_TARGET_ANDROID
             - EGL_NATIVE_BUFFER_ANDROID


          - Display Extensions

             - EGL_NATIVE_PIXMAP_KHR
                    / DirectFB
                    / X11
                    / SDL


          - Impl Extensions              <context>

             - VG_PARENT_IMAGE_KHR                               OPENVG_IMAGE
                    / DVG

             - GL_RENDERBUFFER_KHR
                    / X11EGL

             - GL_TEXTURE_2D_KHR
                    / X11EGL


             - ...
               GL_TEXTURE_3D_KHR
               GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR
               GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR
               GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR
               GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR
               GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR
               GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR


    Attributes

          GL_TEXTURE_LEVEL_KHR
          GL_TEXTURE_ZOFFSET_KHR
          EGL_IMAGE_PRESERVED_KHR



  = Extensions (important/interesting to use/support)

     - Core Extensions

          EXT_buffer_age
          EXT_swap_buffers_with_damage

          EGL_KHR_lock_surface

          EXT_client_extensions
          EXT_platform_base
          EXT_platform_x11

          ANGLE_query_surface_pointer


     - Impl Extensions

          EXT_image_dma_buf_import

          KHR_reusable_sync
          KHR_wait_sync

          KHR_surfaceless_context

          ANDROID_native_fence_sync
*/



namespace DirectFB {


namespace EGL {


class Types : public Direct::Types<Types,Graphics::Core>
{
};


class Core;
class CoreExtension;
class CoreModule;
class DisplayImplementation;

class Display;
class DisplayDFB;
class Config;
class Context;
class Surface;


namespace KHR {
     class Image;
}


}

}


#include "egl_core.h"

#include "egl_display.h"

#include "egl_surface.h"

#include "egl_context.h"

#include "egl_config.h"

#include "egl_option.h"

#include "egl_util.h"

#include "egl_tls.h"

#include "egl_library.h"


namespace DirectFB {

extern Direct::TLSObject2<EGL::TLS> EGLTLS;

}


#endif

#endif

