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

#ifndef ___DFBEGL__tls__H___
#define ___DFBEGL__tls__H___

#include "dfbegl.h"


#ifdef __cplusplus
extern "C" {
#endif


// C wrappers


#ifdef __cplusplus
}

#include <direct/Map.h>
#include <direct/String.h>
#include <direct/TLSObject.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>


#include <list>
#include <map>


namespace DirectFB {


namespace EGL {


/*
 * TODO: Add error checking and reporting helpers for applications
 */


class TLS
{
     friend class Direct::TLSObject2<TLS>;


     static TLS *create( void *ctx, void *params )
     {
          return new TLS();
     }

     static void destroy( void *ctx, TLS *tls )
     {
          delete tls;
     }

     TLS();

public:
     EGLint GetError();
     void   SetError( EGLint egl_error );

     EGLenum        GetAPI();
     void           SetAPI( EGLenum api );

     Context       *GetContext();
     void           SetContext( Context *context );

     Surface       *GetDraw();
     void           SetDraw( Surface *context );

     Surface       *GetRead();
     void           SetRead( Surface *context );

private:
     EGLint         egl_error;
     EGLenum        api;
     Context       *context;
     Surface       *draw;
     Surface       *read;
};


}

}


#endif

#endif

