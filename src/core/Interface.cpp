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

#include <config.h>

#include "Interface.h"


D_DEBUG_DOMAIN( DirectFB_Interface,  "DirectFB/Interface",  "DirectFB Interface" );
D_DEBUG_DOMAIN( DirectFB_CallBuffer, "DirectFB/CallBuffer", "DirectFB CallBuffer" );

/*********************************************************************************************************************/

namespace DirectFB {


CallBuffer::CallBuffer( size_t buffer_min )
     :
     magic(0),
     buffer( NULL ),
     buffer_min( buffer_min ),
     buffer_size( 0 ),
     buffer_len( 0 ),
     buffer_prepared( 0 )
{
     D_DEBUG_AT( DirectFB_CallBuffer, "CallBuffer::%s( %p, min %zu )\n", __FUNCTION__, this, buffer_min );

     D_MAGIC_SET( this, CallBuffer );
}

CallBuffer::~CallBuffer()
{
     D_DEBUG_AT( DirectFB_CallBuffer, "CallBuffer::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, CallBuffer );

     CoreTLS *core_tls = Core_GetTLS();

     if (core_tls) {
          if (core_tls->call_buffer == this)
               core_tls->call_buffer = NULL;
     }
     else
          D_WARN( "TLS error" );

     D_ASSUME( buffer_len == 0 );

     if (buffer) {
          D_ASSERT( buffer_size > 0 );

          D_DEBUG_AT( DirectFB_CallBuffer, "  -> freeing buffer at %p\n", buffer );

          D_FREE( buffer );
     }

     D_MAGIC_CLEAR( this );
}

void *
CallBuffer::prepare( int    method,
                     size_t len )
{
     D_DEBUG_AT( DirectFB_CallBuffer, "CallBuffer::%s( %p, method %d, len %zu )\n", __FUNCTION__, this, method, len );

     D_MAGIC_ASSERT( this, CallBuffer );

     if (buffer_len == 0) {
          CoreTLS *core_tls = Core_GetTLS();

          if (core_tls) {
               if (core_tls->call_buffer != this) {
                    if (core_tls->call_buffer) {
                         CallBuffer *last_buffer = (CallBuffer *) core_tls->call_buffer;

                         last_buffer->flush( false );
                    }

                    core_tls->call_buffer = this;
               }
          }
          else
               D_WARN( "TLS error" );
     }
#if D_DEBUG_ENABLED
     else {
          CoreTLS *core_tls = Core_GetTLS();

          if (core_tls)
               D_ASSERT( core_tls->call_buffer == this );
          else
               D_WARN( "TLS error" );
     }
#endif

     D_ASSERT( buffer_prepared == buffer_len );

     size_t total = sizeof(int) * 2 + ((len+3) & ~3);

     if (buffer_len + total > buffer_size) {
          D_DEBUG_AT( DirectFB_CallBuffer, "  -> need %zu > %zu bytes!\n", buffer_len + total, buffer_size );

          flush( false );

          if (!buffer) {
               buffer_size = MAX( total, buffer_min );

               buffer = D_MALLOC( buffer_size );
               if (!buffer) {
                    buffer_size = 0;
                    return NULL;
               }

               D_DEBUG_AT( DirectFB_CallBuffer, "  -> allocated buffer at %p (%zu bytes)\n", buffer, buffer_size );
          }
          else {
               D_ASSERT( buffer_size >= buffer_min );

               D_DEBUG_AT( DirectFB_CallBuffer, "  -> reusing buffer at %p (%zu bytes)\n", buffer, buffer_size );
          }
     }

     buffer_prepared += total;


     u32 *ptr = (u32*) ((u8*) buffer + buffer_len);

     ptr[0] = total;
     ptr[1] = method;

     return ptr + 2;
}

void
CallBuffer::commit( size_t len )
{
     D_DEBUG_AT( DirectFB_CallBuffer, "CallBuffer::%s( %p, len %zu )\n", __FUNCTION__, this, len );

     D_MAGIC_ASSERT( this, CallBuffer );

     D_ASSERT( buffer_prepared > buffer_len );

     if (len == 0)
          buffer_len = buffer_prepared;
     else {
          size_t total = sizeof(int) * 2 + ((len+3) & ~3);

          D_ASSERT( buffer_len + total <= buffer_prepared );

          buffer_len      += total;
          buffer_prepared  = buffer_len;
     }

     D_DEBUG_AT( DirectFB_CallBuffer, "  -> len %zu\n", buffer_len );

     if (dfb_config->always_flush_callbuffer) {
          D_DEBUG_AT( DirectFB_CallBuffer, "  -> always-flush-callbuffer option is set, flushing...\n" );

          flush( false );
     }
}

DFBResult
CallBuffer::flush( bool leave_tls )
{
     D_DEBUG_AT( DirectFB_CallBuffer, "CallBuffer::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, CallBuffer );

     if (buffer_len > 0) {
          DFBResult ret;

          ret = flushCalls();
          if (ret) {
              D_DEBUG_AT( DirectFB_CallBuffer, "  -> flushCalls() failed!\n" );
              return ret;
          }

          D_DEBUG_AT( DirectFB_CallBuffer, "  -> flushed\n" );

          if (buffer_size > buffer_min) {
               D_DEBUG_AT( DirectFB_CallBuffer, "  -> freeing oversize buffer at %p\n", buffer );

               D_FREE( buffer );

               buffer      = NULL;
               buffer_size = 0;
          }

          buffer_len      = 0;
          buffer_prepared = 0;
     }

     if (leave_tls) {
          CoreTLS *core_tls = Core_GetTLS();
     
          if (core_tls) {
               if (core_tls->call_buffer == this)
                    core_tls->call_buffer = NULL;
          }
          else
               D_WARN( "TLS error" );
     }

     return DFB_OK;
}


}
