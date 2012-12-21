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

#include <config.h>

#include "Util.h"

extern "C" {
#include <stdlib.h>
#include <string.h>

#include <directfb_strings.h>
}


namespace DirectFB {

namespace Util {


std::string
PrintF( const char *format, ... )
{
     va_list  args;
     size_t   len;
     char     buf[200];
     char    *ptr = buf;

     va_start( args, format );
     len = vsnprintf( buf, sizeof(buf), format, args );
     va_end( args );

     if (len < 0)
          abort();

     if (len >= sizeof(buf)) {
          ptr = (char*) malloc( len+1 );
          if (!ptr)
               abort();

          va_start( args, format );
          len = vsnprintf( ptr, len+1, format, args );
          va_end( args );

          if (len < 0) {
               free( ptr );
               abort();
          }
     }

     std::string str( ptr );

     if (ptr != buf)
          free( ptr );

     return str;
}

std::string
DFBAccelerationMask_Name( DFBAccelerationMask accel )
{
     static const DirectFBAccelerationMaskNames(accelerationmask_names);

     std::string ret;

     for (int i=0, n=0; accelerationmask_names[i].mask; i++) {
          if (accel & accelerationmask_names[i].mask)
               ret += PrintF( "%s%s", n++ ? "," : "", accelerationmask_names[i].name );
     }

     return ret;
}

}

}

