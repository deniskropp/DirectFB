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



D_LOG_DOMAIN( DFBEGL_Config, "DFBEGL/Config", "DirectFB EGL Config" );


namespace DirectFB {

namespace EGL {


Config::Config( Display          *display,
                Graphics::Config *gfx_config )
     :
     display( display ),
     gfx_config( gfx_config )
{
     D_DEBUG_AT( DFBEGL_Config, "EGL::Config::%s( %p, display %p, gfx_config %p )\n",
                 __FUNCTION__, this, display, gfx_config );

}

Config::~Config()
{
}

EGLint
Config::GetAttrib( EGLint attribute, EGLint *value )
{
     D_DEBUG_AT( DFBEGL_Config, "EGL::Config::%s( %p, attribute 0x%08x (%d) '%s' )\n",
                 __FUNCTION__, this, attribute, attribute, *ToString<EGLInt>( EGLInt(attribute) ) );

     if (gfx_config) {
          long v;

          if (gfx_config->GetOption( ToString<EGLInt>( EGLInt(attribute) ), v ) == DFB_OK) {
               *value = v;
               return EGL_SUCCESS;
          }
     }

     return EGL_BAD_ATTRIBUTE;
}


}

}

