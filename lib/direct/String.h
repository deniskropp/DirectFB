/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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
#include <direct/Types++.h>

extern "C" {
#endif

#include <direct/compiler.h>
#include <direct/log.h>
#include <direct/log_domain.h>


// C Wrapper

__dfb_no_instrument_function__
D_String   *D_String_NewEmpty( void );

__dfb_no_instrument_function__
size_t      D_String_PrintF( D_String *str, const char *format, ... )           D_FORMAT_PRINTF(2);

__dfb_no_instrument_function__
size_t      D_String_PrintV( D_String *str, const char *format, va_list args );

__dfb_no_instrument_function__
const char *D_String_Buffer( D_String *str );

__dfb_no_instrument_function__
size_t      D_String_Length( D_String *str );

__dfb_no_instrument_function__
void        D_String_Delete( D_String *str );

__dfb_no_instrument_function__
const char *D_String_CopyTLS( D_String *str );

__dfb_no_instrument_function__
const char *D_String_PrintTLS( const char *format, ... )           D_FORMAT_PRINTF(1);

#ifdef __cplusplus
}

#include <ostream>
#include <string>
#include <vector>


namespace Direct {


template <class _CharT = char>
class StringBase
{
private:
     std::basic_string<_CharT>  _str;
     std::basic_string<_CharT> &str;

public:
     StringBase()
          :
          str( _str )
     {
     }

     StringBase( const StringBase<_CharT> &other )
          :
          _str( other.str ),
          str( _str )
     {
     }

     StringBase( std::string &str )
          :
          str( str )
     {
     }

     StringBase( const _CharT *str )
          :
          _str( str ),
          str( _str )
     {
     }

     /*
      * Copies the string to a TLS buffer using fixed number of 32 buffers used in a circular way, so it should
      * fit even for the heaviest legacy print (in C without management of String objects in parameter list).
      */
     const _CharT *CopyTLS();


     StringBase &
     PrintF( const _CharT *format, ... )          D_FORMAT_PRINTF(2);

     StringBase &
     PrintF( const _CharT *format, va_list args, size_t stack_buffer = 300 );

     static StringBase F( const _CharT *format, ... ) D_FORMAT_PRINTF(1);

     void
     Clear();

     StringsBase<_CharT>
     GetTokens( const StringBase<_CharT> &delimiter ) const;



     inline std::string &
     string()
     {
          return str;
     }

     inline const _CharT *
     buffer() const
     {
          return str.c_str();
     }

     inline size_t
     length() const
     {
          return str.size();
     }


     /*
      * Use
      */

     inline operator const std::string& () const {
          return str;
     }

     inline const _CharT * operator *() const {
          return buffer();
     }

     inline bool operator ==(const _CharT *buf) const {
          return !strcmp( buffer(), buf );
     }


     /*
      * Assign
      */

     inline StringBase& operator= (const _CharT *buf) {
          str = buf;
          return *this;
     }

     inline StringBase& operator= (const StringBase &other) {
          _str = other.str;
          str = _str;
          return *this;
     }


     /*
      * Append
      */

     inline StringBase& operator+= (const StringBase &other) {
          str.append( other.str );
          return *this;
     }

     inline StringBase& operator+= (const _CharT *buf) {
          str.append( buf );
          return *this;
     }

     inline StringBase operator+ (const _CharT *buf) {
          StringBase<_CharT> result = *this;
          result.str.append( buf );
          return result;
     }


     /*
      * Streams
      */

     friend std::ostream &operator << (std::ostream &stream, const StringBase<_CharT> &string) {
          stream << string.str;
          return stream;
     }

     friend DirectLog *operator << (DirectLog *log, const StringBase<_CharT> &string) {
          direct_log_write( log, string.buffer(), string.length() );
          return log;
     }

     friend DirectLogDomain &operator << (DirectLogDomain &domain, const StringBase<_CharT> &string) {
          direct_log_domain_log( &domain, DIRECT_LOG_VERBOSE, __FUNCTION__, __FILE__, __LINE__, "%s", string.buffer() );
          return domain;
     }
};


template <class _CharT = char>
class StringsBase : public std::vector<StringBase<_CharT> >
{
public:
     Direct::String
     Concatenated( const Direct::String &space ) const
     {
          Direct::String  result;
          const char     *s = "";

          for (typename StringsBase::const_iterator it = this->begin(); it != this->end(); it++) {
               result.PrintF( "%s%s", s, **it );

               s = *space;
          }

          return result;
     }
};



}


#endif // __cplusplus

#endif

