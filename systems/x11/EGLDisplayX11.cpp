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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <xcb/xcb.h>

extern "C" {
#include <direct/messages.h>

#include <core/system.h>

#include <idirectfb.h>
}

#include <direct/ToString.h>

#include <core/CoreDFB.h>

#include "EGLDisplayX11.h"


D_LOG_DOMAIN( DFBX11_EGLCoreModule,    "DFBX11/EGLCoreModule",    "DirectFB X11 EGL Core Module" );
D_LOG_DOMAIN( DFBX11_EGLDisplay,       "DFBX11/EGLDisplay",       "DirectFB X11 EGL Display" );


namespace DirectFB {

namespace X11 {

using namespace std::placeholders;


__attribute__((constructor))
void dfb_x11_egl_core_register( void );


void
dfb_x11_egl_core_register()
{
     static EGLCoreModuleX11 egl_core_module;

     direct_modules_register( &DirectFB::EGL::core_modules,
                              DFBEGL_CORE_ABI_VERSION,
                              "dfbegl_core_x11", &egl_core_module );
}


/**********************************************************************************************************************/

namespace EGL {

Image::Image( DirectFB::EGL::KHR::Image &egl_image,
              EGLDisplayX11             &display )
     :
     pixmap( 0 ),
     allocation( NULL )
{
     D_LOG( DFBX11_EGLDisplay, DEBUG_1, "X11::Image::%s( %p, KHR::Image %p )\n", __FUNCTION__, this, &egl_image );

     D_LOG( DFBX11_EGLDisplay, VERBOSE, "  -> Initialising EGLImageKHR from Pixmap (image %p, Pixmap %p)\n", &egl_image, egl_image.buffer );

     pixmap = (Pixmap) egl_image.buffer;


     XWindowAttributes  attrs;

     XGetWindowAttributes( display.x11_display, pixmap, &attrs );


     DFBResult             ret;
     DFBSurfaceDescription desc;

     desc.flags  = (DFBSurfaceDescriptionFlags)( DSDESC_WIDTH | DSDESC_HEIGHT );
     desc.width  = attrs.width;//egl_image.gfx_options.GetValue( "WIDTH",  );
     desc.height = attrs.height;//egl_image.gfx_options.Get( "HEIGHT" );

     ret = idirectfb_singleton->CreateSurface( idirectfb_singleton, &desc, &egl_image.surface );
     if (ret) {
          D_DERROR_AT( DFBX11_EGLDisplay, ret, "  -> IDirectFB::CreateSurface( %dx%d ) failed!\n", desc.width, desc.height );
          return;
     }

     ret = egl_image.surface->Allocate( egl_image.surface, DSBR_FRONT, DSSE_LEFT, "Pixmap/X11", pixmap, &allocation );
     if (ret) {
          D_DERROR_AT( DFBX11_EGLDisplay, ret, "  -> IDirectFBSurface::Allocate( FRONT, LEFT, 'Pixmap/X11' ) failed!\n" );
          return;
     }
}

}

/**********************************************************************************************************************/

DFBResult
EGLCoreModuleX11::Display_Probe( const DirectFB::EGL::Display &display,
                                 unsigned int       &ret_score )
{
     D_DEBUG_AT( DFBX11_EGLDisplay, "EGLDisplayX11::%s( %p, display %p, native_display 0x%08lx )\n",
                 __FUNCTION__, this, &display, (unsigned long) display.native_display );

     if (display.native_display == idirectfb_singleton) {
          D_DEBUG_AT( DFBX11_EGLDisplay, "  -> is IDirectFB singleton = %p!\n", idirectfb_singleton );
          return DFB_UNSUPPORTED;
     }

     ret_score = 0;

     if (display.native_display != 0) {
//          ret_score = 10;

          if (getenv("DISPLAY")) {
               D_DEBUG_AT( DFBX11_EGLDisplay, "  -> DISPLAY is set (%s)\n", getenv("DISPLAY") );

               ret_score = 50;
          }
     }

     return DFB_OK;
}

DFBResult
EGLCoreModuleX11::Display_Initialise( EGLDisplayX11 &display )
{
     D_DEBUG_AT( DFBX11_EGLDisplay, "EGLDisplayX11::%s( %p, display %p, x11_display %p )\n",
                 __FUNCTION__, this, &display, display.x11_display );

     DirectFB::EGL::KHR::Image::Register< DirectFB::EGL::KHR::Image::Initialise >( display.GetName() / DirectFB::EGL::EGLInt(EGL_NATIVE_PIXMAP_KHR),
                                                     std::bind( &EGLDisplayX11::Image_Initialise, &display, _1 ) );

     DirectFB::EGL::Surface::Register< DirectFB::EGL::Surface::Initialise >( display.GetName(),
                                                         std::bind( &EGLDisplayX11::Surface_Initialise, &display, _1 ) );

     SurfaceXWindow::RegisterConversion< DirectFB::EGL::Surface, EGLDisplayX11& >( display );

     X11::EGL::Image::RegisterConversion< DirectFB::EGL::KHR::Image, EGLDisplayX11& >( display );

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
EGLDisplayX11::Image_Initialise( DirectFB::EGL::KHR::Image &image )
{
     D_DEBUG_AT( DFBX11_EGLCoreModule, "EGLDisplayX11::%s( %p, image %p )\n", __FUNCTION__, this, &image );

     // IDirectFBSurface::Allocate...

     return DFB_OK;
}

DFBResult
EGLDisplayX11::Surface_Initialise( SurfaceXWindow &surface )
{
     D_DEBUG_AT( DFBX11_EGLDisplay, "EGL::SurfaceXWindow::%s( %p ) <- window 0x%08lx\n",
                 __FUNCTION__, this, surface.window );

     DFBResult ret;


     ::Window              window = surface.window;
     bool                  is_pixmap = false;
     DFBSurfaceDescription desc;

     XWindowAttributes  attrs;

     //     xcb_generic_error_t *err = NULL;
     //
     //     D_DEBUG_AT( DFBX11_EGLDisplay, "  -> calling xcb_get_window_attributes...\n" );
     //     xcb_get_window_attributes_cookie_t  cookie = xcb_get_window_attributes( (xcb_connection_t*)display->x11_display, window );
     //
     //     D_DEBUG_AT( DFBX11_EGLDisplay, "  -> calling xcb_get_window_attributes_reply...\n" );
     //     xcb_get_window_attributes_reply_t  *reply  = xcb_get_window_attributes_reply( (xcb_connection_t*)display->x11_display, cookie, &err );
     //
     //     if (!reply) {
     //          D_ERROR( "DFBEGL/SurfaceXWindow: xcb_get_window_attributes() failed (error code %d)!\n", err ? err->error_code : 0 );
     //          return DFB_FAILURE;
     //     }

     XMapWindow( x11_display, window );
     XSync( x11_display, False );
     XGetWindowAttributes( x11_display, window, &attrs );

     desc.width  = attrs.width;
     desc.height = attrs.height;

     D_DEBUG_AT( DFBX11_EGLDisplay, "  -> size %dx%d\n", desc.width, desc.height );

     desc.flags       = (DFBSurfaceDescriptionFlags)(DSDESC_WIDTH       | DSDESC_HEIGHT |
                                                     /*DSDESC_PIXELFORMAT |*/ DSDESC_CAPS   |
                                                     DSDESC_RESOURCE_ID | DSDESC_HINTS);
     desc.pixelformat = DSPF_ARGB;
     desc.caps        = DSCAPS_SHARED;
     desc.resource_id = window;
     desc.hints       = DSHF_NONE;

     if (surface.parent.native_handle.clazz == DirectFB::EGL::NativeHandle::CLASS_WINDOW) {
          D_FLAGS_SET( desc.caps, DSCAPS_PRIMARY );
          D_FLAGS_SET( desc.hints, DSHF_WINDOW );
     }
     else {
          is_pixmap = true;
     }

     IDirectFB *dfb = parent.GetDFB();

     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

     ret = dfb->CreateSurface( dfb, &desc, &surface.parent.surface );
     if (ret) {
          D_DERROR( ret, "DFBEGL/SurfaceXWindow: IDirectFB::CreateSurface() failed!\n" );
     }

     ret = surface.parent.surface->Allocate( surface.parent.surface, DSBR_BACK, DSSE_LEFT,
                                             is_pixmap ? "Pixmap/X11" : "Window/X11", window, &surface.allocation );
     if (ret) {
          D_DERROR( ret, "DFBEGL/SurfaceXWindow: IDirectFBSurface::Allocate() failed!\n" );
          surface.parent.surface->Release( surface.parent.surface );
          surface.parent.surface = NULL;
          return ret;
     }

     return surface.parent.Init();
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

DFBResult
EGLCoreModuleX11::Initialise( DirectFB::EGL::Core &core )
{
     D_DEBUG_AT( DFBX11_EGLCoreModule, "EGLCoreModuleX11::%s( %p, core %p )\n",
                 __FUNCTION__, this, &core );

     DirectFB::EGL::Core::Register< DirectFB::EGL::Display::Probe >     ( EGLDisplayX11::GetTypeInstance().GetName(), std::bind( &EGLCoreModuleX11::Display_Probe, this, _1, _2 ) );
     DirectFB::EGL::Core::Register< DirectFB::EGL::Display::Initialise >( EGLDisplayX11::GetTypeInstance().GetName(), std::bind( &EGLCoreModuleX11::Display_Initialise, this, _1 ) );

     EGLDisplayX11::RegisterConversion< DirectFB::EGL::Display, EGLCoreModuleX11& >( *this );

     DirectFB::EGL::Display::Register< DirectFB::EGL::EGLExtension::GetNames >( GetName(), [](){ return "DIRECTFB_display_x11"; } );

     return DFB_OK;
}

/**********************************************************************************************************************/

EGLDisplayX11::EGLDisplayX11( DirectFB::EGL::Display     &display,
                              EGLCoreModuleX11 &module )
     :
     Type( display ),
     x11_display( (::Display*) display.native_display )
{
     D_DEBUG_AT( DFBX11_EGLDisplay, "EGLDisplayX11::%s( %p, native_display 0x%08lx )\n", __FUNCTION__, this, (unsigned long) display.native_display );
}

EGLDisplayX11::~EGLDisplayX11()
{
     D_DEBUG_AT( DFBX11_EGLDisplay, "EGLDisplayX11::%s( %p )\n", __FUNCTION__, this );
}

/**********************************************************************************************************************/

SurfaceXWindow::SurfaceXWindow( DirectFB::EGL::Surface  &surface,
                                EGLDisplayX11 &display )
     :
     Type( surface ),
     window( surface.native_handle.value ),
     allocation( NULL )
{
     D_DEBUG_AT( DFBX11_EGLDisplay, "EGL::SurfaceXWindow::%s( %p, window 0x%08lx )\n",
                 __FUNCTION__, this, window );
}

SurfaceXWindow::~SurfaceXWindow()
{
     D_DEBUG_AT( DFBX11_EGLDisplay, "EGL::SurfaceXWindow::%s( %p )\n",  __FUNCTION__, this );

     if (allocation)
          allocation->Release( allocation );
}


}

}

