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

#ifndef ___DFBEGL__util__H___
#define ___DFBEGL__util__H___

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
 * Utils
 */

class Util {
public:
     static Direct::String APIToString( EGLenum api, EGLint version );
     static EGLenum        StringToAPI( const Direct::String &api );

     static DFBResult      GetOptions( Graphics::Options &options,
                                       const EGLint      *attrib_list );

     static DFBResult      GetSurfaceAttribs( Graphics::Options   &options,
                                              std::vector<EGLint> &attribs );

     static DFBResult      GetSurfaceDescription( Graphics::Options     &options,
                                                  DFBSurfaceDescription &desc );
};


#define DFB_EGL_ATTRIB_LIST_DEBUG_AT(domain,__v)                                                                                            \
     do {                                                                                                                                   \
          if ((__v) != NULL) {                                                                                                              \
               for (const EGLint *v = (__v); *v != EGL_NONE; v++) {                                                                    \
                    D_DEBUG_AT( domain, "  -> %02ld: 0x%08x (%d) '%s'\n",                                                                   \
                                (long)(v - (__v)), *v, *v, (*v >= 0x3000 && *v < 0x4000) ? *ToString<EGL::EGLInt>( EGL::EGLInt(*v) ) : "" );\
               }                                                                                                                            \
          }                                                                                                                                 \
     } while (0)


#define DFB_EGL_RETURN(error, val) { tls->SetError( (error) ); return val; }


}

}


#endif

#endif

