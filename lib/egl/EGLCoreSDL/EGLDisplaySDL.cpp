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

#include <SDL/SDL_video.h>

#include <direct/Types++.h>

extern "C" {
#include <direct/messages.h>

#include <core/system.h>

#include <idirectfb.h>
}

#include <direct/ToString.h>

#include <core/CoreDFB.h>

#include "EGLDisplaySDL.h"


D_LOG_DOMAIN( DFBSDL_EGLCoreModule,    "DFBSDL/EGLCoreModule",    "DirectFB SDL EGL Core Module" );
D_LOG_DOMAIN( DFBSDL_EGLDisplay,       "DFBSDL/EGLDisplay",       "DirectFB SDL EGL Display" );


/**********************************************************************************************************************/

extern "C" {
// FIXME: copied form SDL_DirectFB_video.c
struct private_hwdata {
     IDirectFBSurface *surface;
     IDirectFBPalette *palette;
};
}

/**********************************************************************************************************************/

namespace DirectFB {

namespace SDL {

using namespace std::placeholders;


__attribute__((constructor))
void dfb_sdl_egl_core_register( void );


void
dfb_sdl_egl_core_register()
{
     D_DEBUG_AT( DFBSDL_EGLCoreModule, "%s()\n", __FUNCTION__ );

     EGLCoreModuleSDL *egl_core_module = new EGLCoreModuleSDL();

     direct_modules_register( &DirectFB::EGL::core_modules,
                              DFBEGL_CORE_ABI_VERSION,
                              "dfbegl_core_sdl", egl_core_module );
}

/**********************************************************************************************************************/

DFBResult
EGLCoreModuleSDL::Initialise( DirectFB::EGL::Core &core )
{
     D_DEBUG_AT( DFBSDL_EGLCoreModule, "EGLCoreModuleSDL::%s( %p, core %p )\n", __FUNCTION__, this, &core );

     DirectFB::EGL::Core::Register< DirectFB::EGL::Display::Probe >     ( "Probe",
                                                                          std::bind( &EGLCoreModuleSDL::Display_Probe, this, _1, _2 ),
                                                                          EGLDisplaySDL::GetTypeInstance().GetName() );
     DirectFB::EGL::Core::Register< DirectFB::EGL::Display::Initialise >( "Initialise",
                                                                          std::bind( &EGLCoreModuleSDL::Display_Initialise, this, _1 ),
                                                                          EGLDisplaySDL::GetTypeInstance().GetName() );

     EGLDisplaySDL::RegisterConversion< DirectFB::EGL::Display, EGLCoreModuleSDL& >( *this );

     DirectFB::EGL::Display::Register< DirectFB::EGL::EGLExtension::GetNames >( "GetNames",
                                                                                [](){ return "DIRECTFB_display_sdl"; },
                                                                                GetName() );

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
EGLCoreModuleSDL::Display_Probe( const DirectFB::EGL::Display &display,
                                 unsigned int                 &ret_score )
{
     D_DEBUG_AT( DFBSDL_EGLDisplay, "EGLDisplaySDL::%s( %p, display %p, native_display 0x%08lx )\n",
                 __FUNCTION__, this, &display, (unsigned long) display.native_display );

     if ((IDirectFB*)display.native_display == idirectfb_singleton) {
          D_DEBUG_AT( DFBSDL_EGLDisplay, "  -> is IDirectFB singleton = %p!\n", idirectfb_singleton );
          return DFB_UNSUPPORTED;
     }

     ret_score = 0;

     if (getenv("SDL_VIDEODRIVER")) {
          D_DEBUG_AT( DFBSDL_EGLDisplay, "  -> SDL_VIDEODRIVER is set (%s)\n", getenv("SDL_VIDEODRIVER") );

          ret_score = 50;
     }

     return DFB_OK;
}

DFBResult
EGLCoreModuleSDL::Display_Initialise( EGLDisplaySDL &display )
{
     D_DEBUG_AT( DFBSDL_EGLDisplay, "EGLDisplaySDL::%s( %p, display %p )\n", __FUNCTION__, this, &display );

     return DFB_OK;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

EGLDisplaySDL::EGLDisplaySDL( DirectFB::EGL::Display &display,
                              EGLCoreModuleSDL       &module )
     :
     Type( display )
{
     D_DEBUG_AT( DFBSDL_EGLDisplay, "EGLDisplaySDL::%s( %p, native_display 0x%08lx )\n", __FUNCTION__, this, (unsigned long) display.native_display );

     DirectFB::EGL::Surface::Register< DirectFB::EGL::Surface::Initialise >( "Initialise",
                                                                             std::bind( &EGLDisplaySDL::Surface_Initialise, this, _1 ),
                                                                             "",
                                                                             &display );

     SurfaceSDLSurface::RegisterConversion< DirectFB::EGL::Surface, EGLDisplaySDL& >( *this );
}

EGLDisplaySDL::~EGLDisplaySDL()
{
     D_DEBUG_AT( DFBSDL_EGLDisplay, "EGLDisplaySDL::%s( %p )\n", __FUNCTION__, this );
}

/**********************************************************************************************************************/

DFBResult
EGLDisplaySDL::Surface_Initialise( SurfaceSDLSurface &surface )
{
     D_DEBUG_AT( DFBSDL_EGLDisplay, "EGL::SurfaceSDLSurface::%s( %p ) <- sdl surface %p\n", __FUNCTION__, this, surface.sdl_surface );

     SDL_Surface           *sdl_surface = (SDL_Surface*) surface.sdl_surface;
     DFBSurfaceDescription  desc;

     desc.width  = sdl_surface->w;
     desc.height = sdl_surface->h;

     D_DEBUG_AT( DFBSDL_EGLDisplay, "  -> size %dx%d\n", desc.width, desc.height );

     desc.flags       = (DFBSurfaceDescriptionFlags)(DSDESC_WIDTH       | DSDESC_HEIGHT |
                                                     DSDESC_PIXELFORMAT | DSDESC_CAPS   |
                                                     DSDESC_HINTS);
     desc.caps        = DSCAPS_SHARED;
     desc.hints       = DSHF_NONE;

     switch (sdl_surface->format->BytesPerPixel) {
          case 4:
               if (sdl_surface->format->Amask == 0xff000000 && sdl_surface->format->Rmask == 0xff0000 && sdl_surface->format->Gmask == 0xff00 && sdl_surface->format->Bmask == 0xff) {
                    desc.pixelformat = DSPF_ARGB;
                    break;
               }
               else if (sdl_surface->format->Amask == 0 && sdl_surface->format->Rmask == 0xff0000 && sdl_surface->format->Gmask == 0xff00 && sdl_surface->format->Bmask == 0xff) {
                    desc.pixelformat = DSPF_RGB32;
                    break;
               }
               D_WARN( "unsupported format" );
               return DFB_UNSUPPORTED;

          case 3:
               if (sdl_surface->format->Rmask == 0xff0000 && sdl_surface->format->Gmask == 0xff00 && sdl_surface->format->Bmask == 0xff) {
                    desc.pixelformat = DSPF_RGB24;
                    break;
               }
               D_WARN( "unsupported format" );
               return DFB_UNSUPPORTED;

          case 2:
               if (sdl_surface->format->Rmask == 0xf800 && sdl_surface->format->Gmask == 0x07e0 && sdl_surface->format->Bmask == 0x1f) {
                    desc.pixelformat = DSPF_RGB16;
                    break;
               }
               else if (sdl_surface->format->Rmask == 0x7c00 && sdl_surface->format->Gmask == 0x03e0 && sdl_surface->format->Bmask == 0x1f) {
                    desc.pixelformat = DSPF_RGB555;
                    break;
               }
               D_WARN( "unsupported format" );
               return DFB_UNSUPPORTED;

          case 1:
               if (sdl_surface->format->Amask == 0xff) {
                    desc.pixelformat = DSPF_A8;
                    break;
               }
               D_WARN( "unsupported format" );
               return DFB_UNSUPPORTED;

          default:
               D_WARN( "unsupported format" );
               return DFB_UNSUPPORTED;
     }

     if (surface.parent.native_handle.clazz == DirectFB::EGL::NativeHandle::CLASS_WINDOW) {
          D_FLAGS_SET( desc.caps, DSCAPS_GL );
          D_FLAGS_SET( desc.caps, DSCAPS_PRIMARY );
//          D_FLAGS_SET( desc.caps, DSCAPS_DOUBLE );
          D_FLAGS_SET( desc.hints, DSHF_WINDOW );
     }

     D_DEBUG_AT( DFBSDL_EGLDisplay, "  -> caps %s\n", *ToString<DFBSurfaceCapabilities>( desc.caps ) );

     surface.parent.surface = sdl_surface->hwdata->surface;

     surface.parent.surface->AddRef( surface.parent.surface );

     surface.parent.surface->AllowAccess( surface.parent.surface, "*" );

     return DFB_OK;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

SurfaceSDLSurface::SurfaceSDLSurface( DirectFB::EGL::Surface &surface,
                                      EGLDisplaySDL          &display )
     :
     Type( surface ),
     display( display ),
     sdl_surface( (SDL_Surface*) surface.native_handle.value )
{
     D_DEBUG_AT( DFBSDL_EGLDisplay, "EGL::SurfaceSDLSurface::%s( %p, sdl surface %p )\n", __FUNCTION__, this, sdl_surface );
}

SurfaceSDLSurface::~SurfaceSDLSurface()
{
     D_DEBUG_AT( DFBSDL_EGLDisplay, "EGL::SurfaceSDLSurface::%s( %p )\n",  __FUNCTION__, this );
}


}

}

