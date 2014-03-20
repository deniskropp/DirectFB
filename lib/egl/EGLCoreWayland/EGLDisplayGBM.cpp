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

#include <gbm.h>

#include <direct/Types++.h>

extern "C" {
#include <direct/messages.h>

#include <core/system.h>

#include <idirectfb.h>
}

#include <direct/ToString.h>

#include <core/CoreDFB.h>

#include "EGLDisplayGBM.h"


D_LOG_DOMAIN( DFBGBM_EGLCoreModule,    "DFBGBM/EGLCoreModule",    "DirectFB GBM EGL Core Module" );
D_LOG_DOMAIN( DFBGBM_EGLDisplay,       "DFBGBM/EGLDisplay",       "DirectFB GBM EGL Display" );

D_LOG_DOMAIN( DFBGBM_BindDisplay,      "DFBGBM/BindDisplay",      "DirectFB GBM EGL BindDisplay extension" );


using namespace std::placeholders;


namespace DirectFB {

namespace GBM {


__attribute__((constructor))
void dfb_gbm_egl_core_register( void );


void
dfb_gbm_egl_core_register()
{
     static EGLCoreModuleGBM egl_core_module;

     direct_modules_register( &EGL::core_modules,
                              DFBEGL_CORE_ABI_VERSION,
                              "dfbegl_core_gbm", &egl_core_module );
}

/**********************************************************************************************************************/

EGLCoreModuleGBM::EGLCoreModuleGBM()
{
     D_DEBUG_AT( DFBGBM_EGLCoreModule, "EGLCoreModuleGBM::%s( %p )\n", __FUNCTION__, this );
}

DFBResult
EGLCoreModuleGBM::Initialise( DirectFB::EGL::Core &core )
{
     D_DEBUG_AT( DFBGBM_EGLCoreModule, "EGLCoreModuleGBM::%s( %p, core %p )\n",
                 __FUNCTION__, this, &core );

     Core::Register< Display::Probe >     ( "Probe",
                                            std::bind( &EGLCoreModuleGBM::Display_Probe, this, _1, _2 ),
                                            EGLDisplayGBM::GetTypeInstance().GetName() );
     Core::Register< Display::Initialise >( "Initialise",
                                            std::bind( &EGLCoreModuleGBM::Display_Initialise, this, _1 ),
                                            EGLDisplayGBM::GetTypeInstance().GetName() );

     EGLDisplayGBM::RegisterConversion< EGL::Display, EGLCoreModuleGBM& >( *this );

     Display::Register< EGLExtension::GetNames >( "GetNames",
                                                  [](){ return "DIRECTFB_display_drm";},
                                                  GetName() );

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
EGLCoreModuleGBM::Display_Probe( const EGL::Display &display,
                                     unsigned int       &ret_score )
{
     D_DEBUG_AT( DFBGBM_EGLCoreModule, "EGLCoreModuleGBM::%s( %p, display %p, native_display 0x%08lx )\n",
                 __FUNCTION__, this, &display, (unsigned long) display.native_display );

     if (display.native_display && display.native_display == idirectfb_singleton) {
          D_DEBUG_AT( DFBGBM_EGLDisplay, "  -> is IDirectFB singleton = %p!\n", idirectfb_singleton );
          return DFB_UNSUPPORTED;
     }

     ret_score = 0;

     void *deref = NULL;

     if (direct_pointer_is_accessible( display.native_display ))
          deref = *(void **) display.native_display;
     else
          D_DEBUG_AT( DFBGBM_EGLCoreModule, "  -> pointer not accessible\n" );

     if (deref == &gbm_device_interface) {
          D_DEBUG_AT( DFBGBM_EGLCoreModule, "  -> *display.native_display == &gbm_device_interface (%p)\n", &gbm_device_interface );

          ret_score = 100;
     }
     else if (display.native_display) {
          D_DEBUG_AT( DFBGBM_EGLCoreModule, "  -> *display.native_display (%p) != &gbm_device_interface (%p)\n", deref, &gbm_device_interface );

          char *env = getenv("EGL_PLATFORM");

          if (env) {
               D_DEBUG_AT( DFBGBM_EGLCoreModule, "  -> EGL_PLATFORM is set to '%s'\n", env );

               if (!strcmp( env, "drm" ))
                    ret_score = 60;
          }
     }

     return DFB_OK;
}

DFBResult
EGLCoreModuleGBM::Display_Initialise( EGLDisplayGBM &display )
{
     D_DEBUG_AT( DFBGBM_EGLCoreModule, "EGLCoreModuleGBM::%s( %p, display %p, wayland_display %p )\n",
                 __FUNCTION__, this, &display, display.gbm_device );

     // GBM Client Initialisation

//     display.parent.native_pixmap_target = EGL_WAYLAND_BUFFER_WL;

     display.gbm_device = (::gbm_device*) display.parent.native_display;

     return DFB_OK;


error:
     // FIXME: cleanup

     return DFB_FAILURE;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

EGLDisplayGBM::EGLDisplayGBM( EGL::Display         &display,
                                      EGLCoreModuleGBM &module )
     :
     Type( display ),
     gbm_device( NULL )
{
     D_DEBUG_AT( DFBGBM_EGLDisplay, "EGLDisplayGBM::%s( %p, native_display 0x%08lx )\n", __FUNCTION__, this, (unsigned long) display.native_display );

     KHR::Image::Register< KHR::Image::Initialise >( "Initialise",
                                                     std::bind( &EGLDisplayGBM::Image_Initialise, this, _1 ),
                                                     EGLInt(EGL_NATIVE_PIXMAP_KHR),
                                                     &display );

     EGL::Surface::Register< EGL::Surface::Initialise >( "Initialise",
                                                         std::bind( &EGLDisplayGBM::Surface_Initialise, this, _1 ),
                                                         "",
                                                         &display );

     SurfaceGBMWindow::RegisterConversion< EGL::Surface, EGLDisplayGBM& >( *this );
}

EGLDisplayGBM::~EGLDisplayGBM()
{
     D_DEBUG_AT( DFBGBM_EGLDisplay, "EGLDisplayGBM::%s( %p )\n", __FUNCTION__, this );
}

/**********************************************************************************************************************/

DFBResult
EGLDisplayGBM::Image_Initialise( DirectFB::EGL::KHR::Image &image )
{

     D_DEBUG_AT( DFBGBM_EGLDisplay, "EGLDisplayGBM::%s( %p, image %p )\n", __FUNCTION__, this, &image );

     D_ASSERT( image.target == EGL_WAYLAND_BUFFER_WL );

     DFBResult         ret;
     gbm_bo           *buffer  = (gbm_bo *) image.buffer;
     IDirectFBSurface *surface = buffer->surface;

     ret = (DFBResult) surface->AddRef( surface );
     if (ret) {
          D_DERROR_AT( DFBGBM_EGLDisplay, ret, "  -> IDirectFBSurface::AddRef() failed!\n" );
          return ret;
     }

     int w, h;

     surface->GetSize( surface, &w, &h );

     D_INFO( "DFBEGL/Image: New EGLImage from WL::Buffer (%dx%d)\n", w, h );

     image.dfb_surface = surface;

     return DFB_OK;
}

DFBResult
EGLDisplayGBM::Surface_Initialise( SurfaceGBMWindow &surface )
{
     D_DEBUG_AT( DFBGBM_EGLDisplay, "EGLDisplayGBM::%s( %p ) <- window %p\n",
                 __FUNCTION__, this, surface.window );

     DFBResult             ret;
     DFBSurfaceDescription desc;

     EGL::Util::GetSurfaceDescription( surface.parent.gfx_options, desc );

     D_FLAGS_SET( desc.flags, DSDESC_PIXELFORMAT );

     desc.pixelformat = DSPF_ARGB;

     if (surface.parent.native_handle.clazz == NativeHandle::CLASS_WINDOW) {
          wl_egl_window *window = surface.window;

          D_FLAGS_SET( desc.flags, DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_RESOURCE_ID | DSDESC_CAPS | DSDESC_HINTS );

          desc.pixelformat = DSPF_ARGB;
          desc.caps        = DSCAPS_SHARED;
          desc.hints       = DSHF_NONE;

          desc.width       = window->width;
          desc.height      = window->height;
          desc.resource_id = window ? wl_proxy_get_id( (wl_proxy*) window->surface ) : 0;

          D_DEBUG_AT( DFBGBM_EGLDisplay, "  -> wayland surface_id %lu\n", desc.resource_id );

          D_FLAGS_SET( desc.caps, DSCAPS_PRIMARY );
          D_FLAGS_SET( desc.caps, DSCAPS_FLIPPING );
          D_FLAGS_SET( desc.hints, DSHF_WINDOW );

          D_DEBUG_AT( DFBGBM_EGLDisplay, "  -> size %dx%d\n", desc.width, desc.height );
     }

     IDirectFB *dfb = parent.GetDFB();

     ret = dfb->CreateSurface( dfb, &desc, &surface.parent.surface );
     if (ret) {
          D_DERROR( ret, "DFBEGL/SurfaceGBMWindow: IDirectFB::CreateSurface() failed!\n" );
          return ret;
     }

     surface.parent.surface->AllowAccess( surface.parent.surface, "*" );

     return DFB_OK;
}

/**********************************************************************************************************************/

SurfaceGBMWindow::SurfaceGBMWindow( EGL::Surface      &surface,
                                        EGLDisplayGBM &display )
     :
     Type( surface ),
     display( display ),
     window( (gbm_surface*) surface.native_handle.value ),
     buffer( NULL )
{
     D_DEBUG_AT( DFBGBM_EGLDisplay, "SurfaceGBMWindow::%s( %p, window %p, display %p )\n",
                 __FUNCTION__, this, window, &display );

     EGL::Surface::Register< EGL::Surface::SwapBuffersFunc >( "SwapBuffers",
                                                              std::bind( &SurfaceGBMWindow::SwapBuffers, this ),
                                                              GetName(),
                                                              &surface );


     // TODO: store gbm_surface for DRMEGLImpl to pick up
}

SurfaceGBMWindow::~SurfaceGBMWindow()
{
     D_DEBUG_AT( DFBGBM_EGLDisplay, "SurfaceGBMWindow::%s( %p )\n",  __FUNCTION__, this );
}


}

}

