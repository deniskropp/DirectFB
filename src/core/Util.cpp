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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include "Util.h"

extern "C" {
#include <stdlib.h>
#include <string.h>

#include <direct/memcpy.h>

#include <directfb_strings.h>
}


namespace DirectFB {

namespace Util {

D_DEBUG_DOMAIN( DirectFB_Util_PacketBuffer, "DirectFB/Util/PacketBuffer", "DirectFB Util Packet Buffer" );


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




PacketBuffer::PacketBuffer( size_t block_size )
     :
     block_size( block_size ),
     length( 0 )
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s( block_size %zu )\n", __FUNCTION__, block_size );

}

PacketBuffer::~PacketBuffer()
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s()\n", __FUNCTION__ );

     for (std::vector<Buffer*>::const_iterator it = buffers.begin(); it != buffers.end(); ++it)
          delete *it;
}

size_t
PacketBuffer::GetLength()
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s() -> %zu\n", __FUNCTION__, length );
     return length;
}

void *
PacketBuffer::GetBuffer( size_t space )
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s( %zu )\n", __FUNCTION__, space );

     size_t count = buffers.size();

     if (count > 0) {
          Buffer *last = buffers[count-1];

          D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> last has %d/%d\n", last->length, last->size );

          if (last->length + space <= last->size)
               return (void*)((u8*) last->ptr + last->length);
     }

     if (space < block_size)
          space = block_size;

     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> allocating %d bytes\n", space );

     Buffer *buffer = new Buffer( space );

     buffers.push_back( buffer );

     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  => ptr %p\n", buffer->ptr );

     return buffer->ptr;
}

void
PacketBuffer::PutBuffer( void *ptr )
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s( ptr %p )\n", __FUNCTION__, ptr );

     size_t len;
     size_t count = buffers.size();

     D_ASSERT( count > 0 );

     Buffer *last = buffers[count-1];

     D_ASSERT( (unsigned long) ptr >= (unsigned long) last->ptr + last->length );
     D_ASSERT( (unsigned long) ptr <= (unsigned long) last->ptr + last->size );

     len = (unsigned long) ptr - (unsigned long) last->ptr;

     length += len - last->length;

     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> length %d -> %d\n", last->length, len );
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> total length %d\n", length );

     last->length = len;
}

void
PacketBuffer::GetData( void *dst, size_t max )
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s( dst %p, max %zu )\n", __FUNCTION__, dst, max );

     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> total length %d\n", length );

     if (max > length)
          max = length;

     for (std::vector<Buffer*>::const_iterator it = buffers.begin(); it != buffers.end(); ++it) {
          Buffer *buffer = *it;

          D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> copy %zu from %p to %p\n", buffer->length, buffer->ptr, dst );

          direct_memcpy( dst, buffer->ptr, buffer->length );

          dst = (void*)((u8*) dst + buffer->length);
     }
}



}

}

