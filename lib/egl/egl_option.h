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

#ifndef ___DFBEGL__option__H___
#define ___DFBEGL__option__H___


#include "dfbegl.h"


#ifdef __cplusplus
extern "C" {
#endif


// C wrappers


#ifdef __cplusplus
}

#include <direct/Map.hxx>
#include <direct/String.h>
#include <direct/TLSObject.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>


#include <list>
#include <map>


namespace DirectFB {


namespace EGL {


/*
 * Graphics::Option implementation for EGLint value based options
 */

class Option : public Graphics::Option<long>
{
public:
     Option( const EGLint option,
             long         value );
     virtual ~Option();

     virtual Direct::String GetName() const;
     virtual Direct::String GetString() const;

     virtual const long    &GetValue() const;
     virtual void           SetValue( const long &value );

private:
     const EGLint option;
     long         value;
};


}

}


#endif

#endif

