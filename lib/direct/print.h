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

#ifndef __DIRECT__PRINT_H__
#define __DIRECT__PRINT_H__


#include <direct/messages.h>

/**********************************************************************************************************************/

// TODO: Add registration for format codes in strings to have ToString() via printf!

/**********************************************************************************************************************/

int DIRECT_API direct_vsnprintf( char *buf, size_t size, const char *format, va_list args );
int DIRECT_API direct_snprintf ( char *buf, size_t size, const char *format, ... )               D_FORMAT_PRINTF(3);

DirectResult DIRECT_API direct_print( char         *buf,
                                      size_t       size,
                                      const char  *format,
                                      va_list      args,
                                      char       **ret_ptr );


/*
     char *out, buf[ FUSION_ENTRY_INFO_NAME_LENGTH ];
 
     D_PRINT_BUF( buf, "CoreFusionZone == " CORE_UUID_FORMAT " ==", CORE_UUID_VALS( &zone->zone.zone_id ), out );

     fusion_shm_pool_create( world, out, 0x10000000, true, &zone->shmpool );

     D_PRINT_FREE( out, buf );
 
 
*/
#define D_PRINT_BUF( _buf, _format, _args, _out )                          \
     direct_print( _buf, sizeof(_buf), _format, _args, &(_out) )

#define D_PRINT_ALLOC( _format, _args, _out )                              \
     direct_print( NULL, 0, _format, _args, &(_out) )

#define D_PRINT_FREE( _out, _buf )                                         \
     do {                                                                  \
          if ((_out) != (_buf))                                            \
               direct_free( _out );                                        \
     } while (0)

#endif
