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

#include "String.h"

extern "C" {
#include <stdarg.h>

#include <direct/mem.h>
#include <direct/print.h>
}

/*********************************************************************************************************************/

namespace Direct {


extern "C" {


D_String *
D_String_NewEmpty()
{
     return new String();
}

size_t
D_String_PrintF( D_String   *str,
                 const char *format, ... )
{
     va_list  args;

     va_start( args, format );
     str->PrintF( format, args );
     va_end( args );

     return str->length();
}

size_t
D_String_PrintV( D_String   *str,
                 const char *format,
                 va_list     args )
{
     str->PrintF( format, args );

     return str->length();
}

const char *
D_String_Buffer( D_String *str )
{
     return str->buffer();
}

size_t
D_String_Length( D_String *str )
{
     return str->length();
}

void
D_String_Delete( D_String *str )
{
     delete str;
}


}



String::String( const char *str )
     :
     str( str )
{
}

String &
String::PrintF( const char *format, ... )
{
     va_list  args;

     va_start( args, format );
     PrintF( format, args );
     va_end( args );

     return *this;
}

String &
String::PrintF( const char *format, va_list args, size_t stack_buffer )
{
     size_t   len;
     char     buf[stack_buffer];
     char    *ptr = buf;

     // TODO: check if va_copy is required here

#ifdef __GNUC__
     va_list ap2;

     va_copy( ap2, args );
     len = direct_vsnprintf( buf, sizeof(buf), format, ap2 );
     va_end( ap2 );
#else
     len = direct_vsnprintf( buf, sizeof(buf), format, args );
#endif
     if (len < 0)
          return *this;

     if (len >= sizeof(buf)) {
          ptr = (char*) direct_malloc( len+1 );
          if (!ptr) {
               D_OOM();
               return *this;
          }

          len = direct_vsnprintf( ptr, len+1, format, args );
          if (len < 0) {
               direct_free( ptr );
               return *this;
          }
     }

     str.append( ptr );

     if (ptr != buf)
          direct_free( ptr );

     return *this;
}

String
String::F( const char *format, ... )
{
     va_list args;
     String  str;

     va_start( args, format );
     str.PrintF( format, args );
     va_end( args );

     return str;
}

void
String::Clear()
{
     str.clear();
}

std::vector<String>
String::GetTokens( const Direct::String &delimiter ) const
{
     std::vector<String> tokens;

     char *buf = direct_strdup( buffer() );
     char *str = buf;
     char *save, *token;

     while ((token = strtok_r( str, delimiter.buffer(), &save ))) {
          str = NULL;

          tokens.push_back( Direct::String::F( "%s", token ) );
     }

     direct_free( buf );

     return tokens;
}



}

