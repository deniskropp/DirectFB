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

#include <direct/Types++.h>

extern "C" {
#include <direct/messages.h>

#include <display/idirectfbsurface.h>
}

#include <direct/ToString.h>

#include <egl/dfbegl.h>


D_LOG_DOMAIN( DFBEGL_Surface, "DFBEGL/Surface", "DirectFB EGL Surface" );


namespace DirectFB {

namespace EGL {


Surface::Surface()
     :
     config( NULL ),
     gfx_config( NULL ),
     surface( NULL ),
     gfx_peer( NULL )
{
     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p )\n", __FUNCTION__, this );
}

Surface::~Surface()
{
     D_DEBUG_AT( DFBEGL_Surface, "EGL::Surface::%s( %p )\n",
                 __FUNCTION__, this );

     if (gfx_peer)
          delete gfx_peer;

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

     IDirectFBSurface_data *data = (IDirectFBSurface_data*) surface->priv;

     ret = gfx_config->CreateSurfacePeer( data->surface, &gfx_options, &gfx_peer );
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

     Dispatch< Surface::SwapBuffersFunc >( "SwapBuffers", this );

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

