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



#ifndef ___DirectFB__PacketBuffer__H___
#define ___DirectFB__PacketBuffer__H___

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/os/mutex.h>

#include <directfb.h>


// C Wrapper


#ifdef __cplusplus
}



#include <list>
#include <map>
#include <string>
#include <vector>


namespace DirectFB {


namespace Util {




class HeapBuffer {
public:
     HeapBuffer( size_t size )
          :
          size( size ),
          length( 0 )
     {
          ptr = direct_malloc( size );
          D_ASSERT( ptr != NULL );
     }

     ~HeapBuffer()
     {
          direct_free( ptr );
     }

     size_t  size;
     size_t  length;
     void   *ptr;
};


template<typename Buffer=HeapBuffer>
class PacketBuffer {
public:
     PacketBuffer( size_t block_size = 0x8000 );

     ~PacketBuffer();

     size_t
     GetLength();

     void *
     GetBuffer( size_t space );

     void
     PutBuffer( void *ptr );

     void
     PutBuffer( size_t len );

     void
     GetData( void *dst, size_t max );

private:
     size_t         block_size;
     size_t         length;

public:
     typedef typename std::vector<Buffer*> buffer_vector;

     buffer_vector  buffers;
};



D_DEBUG_DOMAIN( DirectFB_Util_PacketBuffer, "DirectFB/Util/PacketBuffer", "DirectFB Util Packet Buffer" );

template<typename Buffer>
PacketBuffer<Buffer>::PacketBuffer( size_t block_size )
     :
     block_size( block_size ),
     length( 0 )
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s( block_size %zu )\n", __FUNCTION__, block_size );

}

template<typename Buffer>
PacketBuffer<Buffer>::~PacketBuffer()
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s()\n", __FUNCTION__ );

     for (typename buffer_vector::const_iterator it = buffers.begin(); it != buffers.end(); ++it)
          delete *it;
}

template<typename Buffer>
size_t
PacketBuffer<Buffer>::GetLength()
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s() -> %zu\n", __FUNCTION__, length );
     return length;
}

template<typename Buffer>
void *
PacketBuffer<Buffer>::GetBuffer( size_t space )
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s( %zu )\n", __FUNCTION__, space );

     size_t count = buffers.size();

     if (count > 0) {
          Buffer *last = buffers[count-1];

          D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> last has %zu/%zu\n", last->length, last->size );

          if (last->length + space <= last->size)
               return (void*)((u8*) last->ptr + last->length);
     }

     if (space < block_size)
          space = block_size;

     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> allocating %zu bytes\n", space );

     Buffer *buffer = new Buffer( space );

     buffers.push_back( buffer );

     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  => ptr %p\n", buffer->ptr );

     return buffer->ptr;
}

template<typename Buffer>
void
PacketBuffer<Buffer>::PutBuffer( void *ptr )
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

     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> length %zu -> %zu\n", last->length, len );
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> total length %zu\n", length );

     last->length = len;
}

template<typename Buffer>
void
PacketBuffer<Buffer>::PutBuffer( size_t len )
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s( len %zu )\n", __FUNCTION__, len );

     size_t count = buffers.size();

     D_ASSERT( count > 0 );

     Buffer *last = buffers[count-1];

     D_ASSERT( last->length + len <= last->size );

     length += len;

     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> length %d + %d\n", last->length, len );
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> total length %d\n", length );

     last->length += len;
}

template<typename Buffer>
void
PacketBuffer<Buffer>::GetData( void *dst, size_t max )
{
     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "PacketBuffer::%s( dst %p, max %zu )\n", __FUNCTION__, dst, max );

     D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> total length %d\n", length );

     if (max > length)
          max = length;

     for (typename buffer_vector::const_iterator it = buffers.begin(); it != buffers.end(); ++it) {
          Buffer *buffer = *it;

          D_DEBUG_AT( DirectFB_Util_PacketBuffer, "  -> copy %zu from %p to %p\n", buffer->length, buffer->ptr, dst );

          direct_memcpy( dst, buffer->ptr, buffer->length );

          dst = (void*)((u8*) dst + buffer->length);
     }
}




}


}


#endif // __cplusplus

#endif

