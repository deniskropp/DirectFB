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

#include <config.h>

#include <stdio.h>
#include <string.h>

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <egl/dfbegl.h>


D_LOG_DOMAIN( DFBEGL_Surface, "DFBEGL/Surface", "DirectFB EGL Surface" );


namespace DirectFB {

namespace EGL {


DFBResult
Display::Surface_Initialise( DirectFB::EGL::Surface &surface )
{
     D_DEBUG_AT( DFBEGL_Surface, "EGLImage::InitImage::%s( %p, image %p )\n", __FUNCTION__, this, &surface );

     DFBResult ret;

     if (surface.native_handle.value) {
          surface.surface = (IDirectFBSurface*) surface.native_handle.ptr;
     }
     else {
          DFBSurfaceDescription desc;

          Util::GetSurfaceDescription( surface.gfx_options, desc );

          if (surface.native_handle.clazz == NativeHandle::CLASS_WINDOW) {
               D_FLAGS_SET( desc.caps, DSCAPS_PRIMARY );

               dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );
          }

          ret = dfb->CreateSurface( dfb, &desc, &surface.surface );
          if (ret) {
               D_DERROR( ret, "DFBEGL/Display: IDirectFB::CreateSurface() failed!\n" );
               return ret;
          }
     }

     int w, h;

     surface.surface->GetSize( surface.surface, &w, &h );

     D_INFO( "DFBEGL/Surface: New EGLSurface from %s IDirectFBSurface (%dx%d) with ID %u\n",
             surface.native_handle.value ? "existing" : "new", w, h, surface.GetID() );

     return DFB_OK;
}


Surface::Surface()
     :
     config( NULL ),
     gfx_config( NULL ),
     surface( NULL ),
     gfx_peer( NULL )
{
     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p )\n", __FUNCTION__, this );
}
#if 0
Surface::Surface( Config           *config,
                  const EGLint     *attrib_list,
                  IDirectFBSurface *surface )
     :
     config( config ),
     surface( surface ),
     gfx_config( NULL ),
     gfx_peer( NULL )
{
     DFBResult ret;

     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p, config %p )\n",
                 __FUNCTION__, this, config );

     D_ASSERT( config != NULL );

     gfx_config = config->gfx_config;

     if (attrib_list) {
          ret = EGL::Util::GetOptions( gfx_options, attrib_list );
          if (ret)
               D_DERROR( ret, "DFBEGL/Surface: Failed to get Options from attrib_list!\n" );
     }

     if (surface) {
          ret = (DFBResult) surface->AddRef( surface );
          if (ret) {
               D_DERROR( ret, "DFBEGL/Surface: IDirectFBSurface::AddRef() failed!\n" );
               this->surface = NULL;
          }
     }
}
#endif
Surface::~Surface()
{
     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p )\n",
                 __FUNCTION__, this );

     if (surface)
          surface->Release( surface );
}

DFBResult
Surface::Init()
{
     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p )\n",
                 __FUNCTION__, this );

     DFBResult ret;

     D_ASSERT( surface != NULL );

     ret = gfx_config->CreateSurfacePeer( surface, &gfx_options, &gfx_peer );
     if (ret) {
          D_DERROR( ret, "DFBEGL/Surface: Graphics::Config::CreateSurfacePeer() failed!\n" );
          return ret;
     }

     return DFB_OK;
}

DFBResult
Surface::Copy( Surface *source )
{
     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p, source %p )\n",
                 __FUNCTION__, this, source );

     DFBResult ret;

     D_ASSERT( surface != NULL );
     D_ASSERT( source != NULL );
     D_ASSERT( source->surface != NULL );

     ret = surface->Blit( surface, source->surface, NULL, 0, 0 );
     if (ret) {
          D_DERROR( ret, "DFBEGL/Surface/Copy: IDirectFBSurface::Blit() failed!\n" );
          return ret;
     }

     return DFB_OK;
}

DFBSurfaceID
Surface::GetID()
{
     DFBResult    ret;
     DFBSurfaceID surface_id = 0;

     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p )\n",
                 __FUNCTION__, this );

     if (surface == NULL) {
          D_DEBUG_AT( DFBEGL_Surface, "  -> NO SURFACE\n" );
          return 0;
     }

     ret = surface->GetID( surface, &surface_id );
     if (ret) {
          D_DERROR( ret, "DFBEGL/Surface: IDirectFBSurface::GetID() failed!\n" );
          return ret;
     }

     D_DEBUG_AT( DFBEGL_Surface, "  -> DFBSurfaceID 0x%08x (%u)\n", surface_id, surface_id );

     return surface_id;
}

EGLint
Surface::SwapBuffers()
{
     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p )\n", __FUNCTION__, this );

     if (surface == NULL) {
          D_DEBUG_AT( DFBEGL_Surface, "  -> NO SURFACE\n" );
          return EGL_BAD_SURFACE;
     }

     if (gfx_peer) {
          DFBResult ret;

          ret = gfx_peer->Flip( NULL, DSFLIP_NONE );
          if (ret) {
               D_DERROR( ret, "DFBEGL/Display: Graphics::SurfacePeer::Flip() failed!\n" );
               return EGL_BAD_SURFACE;
          }
     }

//     for (auto f : Map< Surface::SwapBuffersFunc >()) {
     std::map<std::string,Surface::SwapBuffersFunc> &map = Map< Surface::SwapBuffersFunc >();
     for (std::map<std::string,Surface::SwapBuffersFunc>::iterator f = map.begin(); f != map.end(); f++) {
          D_DEBUG_AT( DFBEGL_Surface, "  -> calling SwapBuffers from %s...\n", (*f).first.c_str() );

          EGLint result = (*f).second();

          D_DEBUG_AT( DFBEGL_Surface, "  -> SwapBuffers from %s returned 0x%04x '%s'\n",
                      (*f).first.c_str(), result, *ToString<EGLInt>( result ) );
     }

     return EGL_SUCCESS;
}

EGLint
Surface::GetAttrib( EGLint attribute, EGLint &value )
{
     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p, attribute 0x%08x (%d) '%s' )\n",
                 __FUNCTION__, this,
                 attribute, attribute, *ToString<EGLInt>( EGLInt(attribute) ) );

     if (surface == NULL) {
          D_DEBUG_AT( DFBEGL_Surface, "  -> NO SURFACE\n" );
          return EGL_BAD_SURFACE;
     }


     DFBSurfaceCapabilities  caps;
     EGL::Option            *option;

     switch (attribute) {
          case EGL_CONFIG_ID:
               return config->GetAttrib( attribute, &value );

          case EGL_WIDTH:
               surface->GetSize (surface, &value, NULL);
               break;

          case EGL_HEIGHT:
               surface->GetSize (surface, NULL, &value);
               break;

          case EGL_LARGEST_PBUFFER:
               value = EGL_FALSE;
               break;

          case EGL_VG_COLORSPACE:
               value = EGL_COLORSPACE_LINEAR;
               break;

          case EGL_VG_ALPHA_FORMAT:
               surface->GetCapabilities( surface, &caps );
               value = (caps & DSCAPS_PREMULTIPLIED) ? EGL_ALPHA_FORMAT_PRE : EGL_ALPHA_FORMAT_NONPRE;
               break;

          default:
               option = gfx_options.Get<EGL::Option>( ToString<EGLInt>( EGLInt(attribute) ) );
               if (option) {
                    value = option->GetValue();
                    break;
               }

               value = EGL_UNKNOWN;
               return EGL_BAD_PARAMETER;
     }

     return EGL_SUCCESS;
}

EGLint
Surface::SetAttrib( EGLint attribute, EGLint value )
{
     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p, attribute 0x%08x (%d) '%s', value 0x%08x (%d) '%s' )\n",
                 __FUNCTION__, this,
                 attribute, attribute, *ToString<EGLInt>( EGLInt(attribute) ),
                 value, value, (value >= 0x3000 && value < 0x4000) ?  *ToString<EGLInt>( EGLInt(value) ) : "" );

     return EGL_BAD_PARAMETER;
}


}

}

