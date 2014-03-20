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

#ifndef ___DFBEGL__surface__H___
#define ___DFBEGL__surface__H___

#include "dfbegl.h"


#ifdef __cplusplus
extern "C" {
#endif


// C wrappers


#ifdef __cplusplus
}

#include <direct/String.h>
#include <direct/TLSObject.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>


#include <list>
#include <map>


namespace DirectFB {


namespace EGL {


/*
 * EGL::Surface for wrapping client's native handles
 */

class Surface : public Types::Type<Surface>
{
     friend class Context;
     friend class Display;

public:
     typedef std::function<DFBResult (Surface &surface)> Initialise;
     typedef std::function<EGLint (void)> SwapBuffersFunc;


     NativeHandle           native_handle;

     Config                *config;          // config
     Graphics::Config      *gfx_config;

     Graphics::Options      gfx_options;     // attribs

     IDirectFBSurface      *surface;         // Init

     Surface();

public:
     Surface( Config           *config,
              const EGLint     *attrib_list );
     virtual ~Surface();

     virtual DFBResult      Init();
     virtual DFBResult      Copy( Surface *source );

     virtual DFBSurfaceID   GetID();

     virtual EGLint         SwapBuffers();

     virtual EGLint         GetAttrib( EGLint attribute, EGLint &value );
     virtual EGLint         SetAttrib( EGLint attribute, EGLint value );

     Config                  *GetConfig() const { return config; }
     const Graphics::Options &GetOptions() const { return gfx_options; }
     IDirectFBSurface        *GetSurface() const { return surface; }
     Graphics::SurfacePeer   *GetPeer() const { return gfx_peer; }

protected:
     Graphics::SurfacePeer *gfx_peer;        // Convert

};


}

}


#endif

#endif

