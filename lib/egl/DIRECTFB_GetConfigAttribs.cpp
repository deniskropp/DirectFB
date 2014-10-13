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

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include "DIRECTFB_GetConfigAttribs.h"


D_LOG_DOMAIN( DFBEGL_GetConfigAttribs, "DFBEGL/GetConfigAttribs", "DirectFB EGL GetConfigAttribs extension" );


using namespace std::placeholders;


namespace DirectFB {

namespace EGL {

/**********************************************************************************************************************/

static GetConfigAttribs get_config_attribs;

/**********************************************************************************************************************/
/**********************************************************************************************************************/

GetConfigAttribs::GetConfigAttribs()
{
     D_DEBUG_AT( DFBEGL_GetConfigAttribs, "GetConfigAttribs::%s( %p )\n", __FUNCTION__, this );

     DirectFB::EGL::Display::Register< DirectFB::EGL::EGLExtension::GetNames >( "GetNames",
                                                                                [](){ return "EGL_DIRECTFB_get_config_attribs";},
                                                                                GetName() );

     DirectFB::EGL::Core::Register< DirectFB::EGL::Core::GetProcAddress >( "GetProcAddress",
                                                                           [](const char *){ return (void*) eglGetConfigAttribsDIRECTFB;},
                                                                           "eglGetConfigAttribsDIRECTFB" );

     DirectFB::EGL::Display::Register< GetConfigAttribsDIRECTFB >( "GetConfigAttribsDIRECTFB",
                                                                   std::bind( &GetConfigAttribs::eglGetConfigAttribs, this, _1, _2, _3, _4 ),
                                                                   "" );
}

GetConfigAttribs::~GetConfigAttribs()
{
     D_DEBUG_AT( DFBEGL_GetConfigAttribs, "GetConfigAttribs::%s( %p )\n", __FUNCTION__, this );
}

/**********************************************************************************************************************/

EGLBoolean
GetConfigAttribs::eglGetConfigAttribsDIRECTFB( EGLDisplay           dpy,
                                               EGLNativePixmapType  native,
                                               EGLint              *attribs,
                                               EGLint               max )
{
     D_DEBUG_AT( DFBEGL_GetConfigAttribs, "%s( display %p, native %p, attribs %p, max %d )\n", __FUNCTION__, dpy, native, attribs, max );

     DirectFB::EGL::TLS *tls = DirectFB::EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!attribs)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);


     DirectFB::EGL::Display *display = (DirectFB::EGL::Display*) dpy;

     DFBResult ret = DirectFB::EGL::Display::Call< GetConfigAttribsDIRECTFB >( "GetConfigAttribsDIRECTFB", "" )( *display, native, attribs, max );

     if (ret)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

DFBResult
GetConfigAttribs::eglGetConfigAttribs( EGL::Display        &display,
                                       EGLNativePixmapType  native,
                                       EGLint              *attribs,
                                       EGLint               max )
{
     D_DEBUG_AT( DFBEGL_GetConfigAttribs, "%s( display %p, native %p, attribs %p, max %d )\n", __FUNCTION__, &display, native, attribs, max );

     if (!attribs)
          return DFB_INVARG;

     DFB_EGL_ATTRIB_LIST_DEBUG_AT( DFBEGL_GetConfigAttribs, attribs );

     IDirectFBSurface *surface = (IDirectFBSurface *) native;

     for (EGLint *v=attribs; *v != EGL_NONE; v+=2) {
          if (max > 0 && v-attribs >= max) {
               D_DEBUG_AT( DFBEGL_GetConfigAttribs, "  -> max (%d) reached (%zd)\n", max, v-attribs );
               break;
          }

          EGLint                attribute = v[0];
          EGLint                value     = v[1];
          DFBSurfacePixelFormat format;
          DFBDimension          size;

          D_DEBUG_AT( DFBEGL_GetConfigAttribs, "  -> [%zd] 0x%04x '%s'  <- %d (0x%08x)\n", v-attribs, attribute, **EGLInt(attribute), value, value );

          switch (attribute) {
          case EGL_BUFFER_SIZE:
               surface->GetPixelFormat( surface, &format );
               value = DFB_COLOR_BITS_PER_PIXEL( format );
               break;

          case EGL_ALPHA_SIZE:
               surface->GetPixelFormat( surface, &format );
               value = DFB_ALPHA_BITS_PER_PIXEL( format );
               break;

          case EGL_BLUE_SIZE:
          case EGL_GREEN_SIZE:
          case EGL_RED_SIZE:
               surface->GetPixelFormat( surface, &format );
               value = DFB_COLOR_BITS_PER_PIXEL( format ) / 3;//FIXME
               break;

          //case EGL_DEPTH_SIZE:
          //case EGL_STENCIL_SIZE:
          //case EGL_RENDERABLE_TYPE:

          case EGL_SURFACE_TYPE:
               value = EGL_WINDOW_BIT;  // FIXME
               break;

          case EGL_WIDTH:     // keep? not a config attribute actually
               surface->GetSize( surface, &size.w, &size.h );
               value = size.w;
               break;

          case EGL_HEIGHT:    // keep? not a config attribute actually
               surface->GetSize( surface, &size.w, &size.h );
               value = size.h;
               break;

          default:
               D_DEBUG_AT( DFBEGL_GetConfigAttribs, "  -> UNRECOGNIZED!!!\n" );
               continue;
          }

          D_DEBUG_AT( DFBEGL_GetConfigAttribs, "            => %d (0x%08x)\n", value, value );

          v[1] = value;
     }

     D_DEBUG_AT( DFBEGL_GetConfigAttribs, " --> DONE -------------\n" );

     DFB_EGL_ATTRIB_LIST_DEBUG_AT( DFBEGL_GetConfigAttribs, attribs );

     return DFB_OK;
}


}

}

