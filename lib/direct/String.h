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
#include <direct/log.h>


// C Wrapper

D_String   *D_String_NewEmpty( void );
size_t      D_String_PrintF( D_String *str, const char *format, ... )           D_FORMAT_PRINTF(2);
size_t      D_String_PrintV( D_String *str, const char *format, va_list args );
const char *D_String_Buffer( D_String *str );
size_t      D_String_Length( D_String *str );
void        D_String_Delete( D_String *str );

const char *D_String_CopyTLS( D_String *str );
const char *D_String_PrintTLS( const char *format, ... )           D_FORMAT_PRINTF(1);

#ifdef __cplusplus
}


#include <ostream>
#include <string>
#include <vector>


namespace Direct {


typedef std::vector<String> Strings;


class String
{
private:
     std::string  _str;
     std::string &str;

public:
     String()
          :
          str( _str )
     {
     }

     String( const Direct::String &other )
          :
          _str( other.str ),
          str( _str )
     {
     }

     String( std::string &str )
          :
          str( str )
     {
     }

     String( const char *str )
          :
          _str( str ),
          str( _str )
     {
     }

     /*
      * Copies the string to a TLS buffer using fixed number of 32 buffers used in a circular way, so it should
      * fit even for the heaviest legacy print (in C without management of String objects in parameter list).
      */
     const char *CopyTLS();


     String &
     PrintF( const char *format, ... )          D_FORMAT_PRINTF(2);

     String &
     PrintF( const char *format, va_list args, size_t stack_buffer = 300 );

     static String F( const char *format, ... ) D_FORMAT_PRINTF(1);

     void
     Clear();

     Strings
     GetTokens( const Direct::String &delimiter ) const;



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

     inline operator const std::string& () const {
          return str;
     }

     inline String& operator= (const char *buf) {
          str = buf;
          return *this;
     }

     inline String& operator= (const String &other) {
          _str = other.str;
          str = _str;
          return *this;
     }

     inline String& operator+= (const String &other) {
          str.append( other.str );
          return *this;
     }

     inline String& operator+= (const char *buf) {
          str.append( buf );
          return *this;
     }

     inline String operator+ (const char *buf) {
          Direct::String result = *this;
          result.str.append( buf );
          return result;
     }

     friend std::ostream &operator << (std::ostream &stream, const Direct::String &string) {
          stream << string.str;
          return stream;
     }

     friend DirectLog *operator << (DirectLog *log, const Direct::String &string) {
          direct_log_write( log, string.buffer(), string.length() );
          return log;
     }

     friend DirectLogDomain &operator << (DirectLogDomain &domain, const Direct::String &string) {
          direct_log_domain_log( &domain, DIRECT_LOG_VERBOSE, __FUNCTION__, __FILE__, __LINE__, "%s", string.buffer() );
          return domain;
     }
};


}


#endif // __cplusplus

#endif

