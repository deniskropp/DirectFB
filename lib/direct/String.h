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

#ifndef ___Direct__String__H___
#define ___Direct__String__H___

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/compiler.h>


// C Wrapper

D_String   *D_String_NewEmpty( void );
size_t      D_String_PrintF( D_String *str, const char *format, ... )           D_FORMAT_PRINTF(2);
size_t      D_String_PrintV( D_String *str, const char *format, va_list args );
const char *D_String_Buffer( D_String *str );
size_t      D_String_Length( D_String *str );
void        D_String_Delete( D_String *str );



#ifdef __cplusplus
}


#include <string>


namespace Direct {


class String
{
private:
     std::string str;

public:
     String()
     {
     }

     String( const std::string &str,
             size_t             pos = 0,
             size_t             len = -1 )
          :
          str( str, pos, len )
     {
     }

     String( const char *buf,
             size_t      pos = 0,
             size_t      len = -1 )
          :
          str( buf, pos, len )
     {
     }

     String( const char *format, ... )            D_FORMAT_PRINTF(2);

     String &
     PrintF( const char *format, ... )            D_FORMAT_PRINTF(2);

     String &
     PrintF( const char *format, va_list args, size_t stack_buffer = 300 );


     inline std::string &
     string()
     {
          return str;
     }

     inline const char *
     buffer() const
     {
          return str.c_str();
     }

     inline size_t
     length() const
     {
          return str.size();
     }

     inline operator const std::string& () {
          return str;
     }
};


}


#endif // __cplusplus

#endif

