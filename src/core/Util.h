/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef ___DirectFB__Util__H___
#define ___DirectFB__Util__H___

#ifdef __cplusplus
extern "C" {
#endif

#include <directfb.h>


// C Wrapper


#ifdef __cplusplus
}



#include <list>
#include <map>
#include <string>


namespace DirectFB {


namespace Util {


#define DIRECTFB_UTIL_MAX_STACK    2048


template <typename T>
class TempArray
{
public:
     TempArray( size_t   size,
                const T *source = NULL )
          :
          size( size )
     {
          D_ASSERT( size > 0 );

          if (size > DIRECTFB_UTIL_MAX_STACK / sizeof(T))
               array = new T[size];
          else
               array = fixed;

          if (source != NULL)
               direct_memcpy( array, source, size * sizeof(T) );
     }

     ~TempArray()
     {
          if (array != fixed)
               delete[] array;
     }

     size_t  size;
     T       fixed[DIRECTFB_UTIL_MAX_STACK / sizeof(T)];
     T      *array;
};



std::string PrintF( const char *format, ... )  D_FORMAT_PRINTF( 1 );

}


}


#endif // __cplusplus

#endif

