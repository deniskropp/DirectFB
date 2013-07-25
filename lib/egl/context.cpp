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

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <egl/dfbegl.h>



D_LOG_DOMAIN( DFBEGL_Context, "DFBEGL/Context", "DirectFB EGL Context" );


namespace DirectFB {

namespace EGL {


Context::Context( EGLenum       api,
                  Config       *config,
                  Context      *share,
                  const EGLint *attrib_list )
     :
     api( api ),
     config( config ),
     share( share ),
     gfx_config( NULL ),
     gfx_context( NULL )
{
     DFBResult ret;

     D_DEBUG_AT( DFBEGL_Context, "EGL::Context::%s( %p, config %p, share %p, attrib_list %p )\n",
                 __FUNCTION__, this, config, share, attrib_list );

     D_ASSERT( config != NULL );

     gfx_config = config->gfx_config;

     if (attrib_list) {
          ret = EGL::Util::GetOptions( gfx_options, attrib_list );
          if (ret)
               D_DERROR( ret, "DFBEGL/Context: Failed to get Options from attrib_list!\n" );
     }
}

Context::~Context()
{
     D_DEBUG_AT( DFBEGL_Context, "EGL::Context::%s( %p )\n",
                 __FUNCTION__, this );
}

DFBResult
Context::Init()
{
     DFBResult ret;

     D_DEBUG_AT( DFBEGL_Context, "EGL::Context::%s( %p )\n",
                 __FUNCTION__, this );

     Direct::String egl_api = Util::APIToString( api, gfx_options.GetValue<long>( "CONTEXT_CLIENT_VERSION", 1 ) );

     ret = gfx_config->CreateContext( egl_api, share ? share->gfx_context : NULL, &gfx_options, &gfx_context );
     if (ret) {
          D_DERROR( ret, "DFBEGL/Surface: Graphics::Config::CreateContext() failed!\n" );
          return ret;
     }

     return DFB_OK;
}

EGLint
Context::Bind( Surface *draw, Surface *read )
{
     DFBResult ret;

     D_DEBUG_AT( DFBEGL_Context, "EGL::Context::%s( %p, draw %p, read %p )\n",
                 __FUNCTION__, this, draw, read );

     ret = gfx_context->Bind( draw->gfx_peer, read->gfx_peer );
     if (ret) {
          D_DERROR( ret, "DFBEGL/Context: Graphics::Context::Bind() failed!\n" );
          return EGL_BAD_CONTEXT;
     }

     binding.Set( draw, read );

     return EGL_SUCCESS;
}

void
Context::Unbind()
{
     D_DEBUG_AT( DFBEGL_Context, "EGL::Context::%s( %p )\n", __FUNCTION__, this );

     binding.Reset();

     gfx_context->Unbind();
}

EGLint
Context::GetAttrib( EGLint attribute, EGLint *value )
{
     D_DEBUG_AT( DFBEGL_Context, "EGL::Context::%s( %p, attribute 0x%08x (%d) '%s' )\n",
                 __FUNCTION__, this, attribute, attribute, *ToString<EGLInt>( EGLInt(attribute) ) );

     switch (attribute) {
          case EGL_CONTEXT_CLIENT_TYPE:
               D_DEBUG_AT( DFBEGL_Context, "  -> EGL_CONTEXT_CLIENT_TYPE, api: '%s', enum: %d\n", *gfx_context->GetAPI(), api );
               *value = api;
               return EGL_SUCCESS;

          case EGL_CONTEXT_CLIENT_VERSION:
               *value = gfx_options.GetValue<long>( "CONTEXT_CLIENT_VERSION", 1 );
               D_DEBUG_AT( DFBEGL_Context, "  -> EGL_CONTEXT_CLIENT_VERSION, (from options %d)\n", *value );
               return EGL_SUCCESS;

          case EGL_RENDER_BUFFER:
               *value = gfx_options.GetValue<long>( "RENDER_BUFFER", EGL_SINGLE_BUFFER );
               D_DEBUG_AT( DFBEGL_Context, "  -> EGL_RENDER_BUFFER, (from options %d)\n", *value );
               return EGL_SUCCESS;
          default:
               break;
     }

     if (gfx_context) {
          long v;

          if (gfx_context->GetOption( ToString<EGLInt>( EGLInt(attribute) ), v ) == DFB_OK) {
               *value = v;
               return EGL_SUCCESS;
          }
     }

     return EGL_BAD_ATTRIBUTE;
}

Surface *
Context::GetSurface( EGLint which )
{
     D_DEBUG_AT( DFBEGL_Context, "EGL::Context::%s( %p, which %d '%s' )\n",
                 __FUNCTION__, this, which, *ToString<EGLInt>( EGLInt(which) ) );

     if (binding.active)
          return (which == EGL_DRAW) ? binding.draw : binding.read;

     return NULL;
}

DFBResult
Context::GetProcAddress( const char  *name,
                         void       **result )
{
     D_DEBUG_AT( DFBEGL_Context, "EGL::Context::%s( %p, name '%s' )\n",
                 __FUNCTION__, this, name );

     void *addr = NULL;

     if (gfx_context) {
          DFBResult ret = gfx_context->GetProcAddress( name, addr );

          if (ret == DFB_OK) {
               *result = addr;
               return DFB_OK;
          }
     }
     else
          D_DEBUG_AT( DFBEGL_Context, "  -> no native context bound\n" );

     return DFB_ITEMNOTFOUND;
}


void
Context::Binding::Set( Surface *draw, Surface *read )
{
     D_DEBUG_AT( DFBEGL_Context, "EGL::Context::Binding::%s( %p, draw %p, read %p )\n",
                 __FUNCTION__, this, draw, read );

     this->draw = draw;
     this->read = read;

     active = true;
}

void
Context::Binding::Reset()
{
     D_DEBUG_AT( DFBEGL_Context, "EGL::Context::Binding::%s( %p )\n", __FUNCTION__, this );

     active = false;
}


}

}

