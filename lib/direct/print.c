/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#include <direct/mem.h>
#include <direct/print.h>
#include <direct/system.h>
#include <direct/trace.h>


/**********************************************************************************************************************/

__dfb_no_instrument_function__
int
direct_vsnprintf( char       *buffer,
                  size_t      bytes,
                  const char *format,
                  va_list     args )
{
     return vsnprintf( buffer, bytes, format, args );
}

/**********************************************************************************************************************/

int
direct_snprintf( char       *buffer,
                 size_t      bytes,
                 const char *format,
                 ... )
{
     int     ret;
     va_list args;

     va_start( args, format );
     ret = direct_vsnprintf( buffer, bytes, format, args );
     va_end( args );

     return ret;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

DirectResult
direct_print( char        *buf,
              size_t       size,
              const char  *format,
              va_list      args,
              char       **ret_ptr )
{
     int len = 1;

     *ret_ptr = buf;

     if (buf) {
          buf[0] = 0;

#ifdef __GNUC__
          va_list args_copy;

          va_copy( args_copy, args );
          len = direct_vsnprintf( buf, size, format, args_copy );
          va_end( args_copy );
#else
          len = direct_vsnprintf( buf, size, format, args );
#endif

          if (len < 0)
               return DR_FAILURE;
     }
     else
          size = 0;


     if (len >= (int) size) {
          char *ptr = buf;

          ptr = direct_malloc( len+1 );
          if (!ptr)
               return DR_NOLOCALMEMORY;

          len = direct_vsnprintf( ptr, len+1, format, args );
          if (len < 0) {
               direct_free( ptr );
               return DR_FAILURE;
          }

          *ret_ptr = ptr;
     }

     return DR_OK;
}

