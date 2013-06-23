/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <fusion/build.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/call.h>
#include <fusion/conf.h>
#include <fusion/hash.h>
#include <fusion/shmalloc.h>

#include "fusion_internal.h"


D_DEBUG_DOMAIN( Fusion_Call, "Fusion/Call", "Fusion Call" );


#if FUSION_BUILD_MULTI

#if FUSION_BUILD_KERNEL

/*********************************************************************************************************************/

typedef struct {
     int          magic;

     DirectThread        *thread;
     bool                 dispatcher;

     FusionWorld         *world;

     FusionCallExecute3  *bins;
     int                  bins_num;
     char                *bins_data;
     int                  bins_data_len;
     long long            bins_create_ts;
} CallTLS;

static DirectTLS call_tls_key;

static void
call_tls_destroy( void *arg )
{
     CallTLS *call_tls = arg;

     D_MAGIC_ASSERT( call_tls, CallTLS );

     fusion_world_flush_calls( call_tls->world, 0 );

     D_ASSUME( call_tls->bins_num == 0 );

     D_MAGIC_CLEAR( call_tls );

     D_FREE( call_tls );
}

void
__Fusion_call_init( void )
{
     direct_tls_register( &call_tls_key, call_tls_destroy );
}

void
__Fusion_call_deinit( void )
{
     direct_tls_unregister( &call_tls_key );
}


static CallTLS *
Call_GetTLS( FusionWorld *world )
{
     CallTLS *call_tls;

     call_tls = direct_tls_get( call_tls_key );
     if (!call_tls) {
          call_tls = D_CALLOC( 1, sizeof(CallTLS) + sizeof(FusionCallExecute3) * fusion_config->call_bin_max_num + fusion_config->call_bin_max_data );
          if (!call_tls) {
               D_OOM();
               return NULL;
          }

          DirectThread *self = direct_thread_self();

          if (self)
               call_tls->dispatcher = fusion_dispatcher_tid( world ) == direct_thread_get_tid( self );

          call_tls->thread    = self;
          call_tls->world     = world;
          call_tls->bins      = (FusionCallExecute3*) (call_tls + 1);
          call_tls->bins_data = (char*) (call_tls->bins + fusion_config->call_bin_max_num);

          D_MAGIC_SET( call_tls, CallTLS );

          direct_tls_set( call_tls_key, call_tls );
     }

     D_MAGIC_ASSERT( call_tls, CallTLS );

     return call_tls;
}

/*********************************************************************************************************************/

DirectResult
fusion_call_init (FusionCall        *call,
                  FusionCallHandler  handler,
                  void              *ctx,
                  const FusionWorld *world)
{
     FusionCallNew call_new;

     if (direct_log_domain_check( &Fusion_Call )) // avoid call to direct_trace_lookup_symbol_at
          D_DEBUG_AT( Fusion_Call, "%s( %p, %p <%s>, %p, %p )\n", __FUNCTION__, call, handler,
                      direct_trace_lookup_symbol_at( handler ), ctx, world );

     D_ASSERT( call != NULL );
     D_ASSERT( handler != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     /* Called from others. */
     call_new.handler = handler;
     call_new.ctx     = ctx;

     while (ioctl( world->fusion_fd, FUSION_CALL_NEW, &call_new )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_NEW");

          return DR_FAILURE;
     }

     memset( call, 0, sizeof(FusionCall) );

     /* Store handler, called directly when called by ourself. */
     call->handler = handler;
     call->ctx     = ctx;

     /* Store call and own fusion id. */
     call->call_id   = call_new.call_id;
     call->fusion_id = fusion_id( world );

     /* Keep back pointer to shared world data. */
     call->shared = world->shared;

     D_DEBUG_AT( Fusion_Call, "  -> call id %d\n", call->call_id );

     return DR_OK;
}

DirectResult
fusion_call_init3 (FusionCall         *call,
                   FusionCallHandler3  handler3,
                   void               *ctx,
                   const FusionWorld  *world)
{
     FusionCallNew call_new;

     if (direct_log_domain_check( &Fusion_Call )) // avoid call to direct_trace_lookup_symbol_at
          D_DEBUG_AT( Fusion_Call, "%s( %p, %p <%s>, %p, %p )\n", __FUNCTION__, call, handler3,
                      direct_trace_lookup_symbol_at( handler3 ), ctx, world );

     D_ASSERT( call != NULL );
     D_ASSERT( handler3 != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     /* Called from others. */
     call_new.handler = handler3;
     call_new.ctx     = ctx;

     while (ioctl( world->fusion_fd, FUSION_CALL_NEW, &call_new )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_NEW");

          return DR_FAILURE;
     }

     memset( call, 0, sizeof(FusionCall) );

     /* Store handler, called directly when called by ourself. */
     call->handler3 = handler3;
     call->ctx      = ctx;

     /* Store call and own fusion id. */
     call->call_id   = call_new.call_id;
     call->fusion_id = fusion_id( world );

     /* Keep back pointer to shared world data. */
     call->shared = world->shared;

     D_DEBUG_AT( Fusion_Call, "  -> call id %d\n", call->call_id );

     return DR_OK;
}

DirectResult
fusion_call_init_from( FusionCall        *call,
                       int                call_id,
                       const FusionWorld *world )
{
     D_DEBUG_AT( Fusion_Call, "%s( %p, %d, %p )\n", __FUNCTION__, call, call_id, world );

     D_ASSERT( call != NULL );
     D_ASSERT( call_id != 0 );
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     memset( call, 0, sizeof(FusionCall) );

     /* Store call id. */
     call->call_id = call_id;

     /* Keep back pointer to shared world data. */
     call->shared = world->shared;

     D_DEBUG_AT( Fusion_Call, "  -> call id %d\n", call->call_id );

     return DR_OK;
}

DirectResult
fusion_call_set_name( FusionCall *call,
                      const char *name )
{
     FusionEntryInfo info;

     D_ASSERT( call != NULL );
     D_ASSERT( name != NULL );

     info.type = FT_CALL;
     info.id   = call->call_id;

     direct_snputs( info.name, name, sizeof(info.name) );

     while (ioctl (_fusion_fd( call->shared ), FUSION_ENTRY_SET_INFO, &info)) {
          perror("FUSION_ENTRY_SET_INFO");
          switch (errno) {
               case EINTR:
                    continue;
               case EAGAIN:
                    return DR_LOCKED;
               case EINVAL:
                    D_ERROR ("Fusion/Call: invalid call\n");
                    return DR_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_ENTRY_SET_NAME");

          return DR_FAILURE;
     }

     return DR_OK;
}

DirectResult
fusion_call_execute (FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *call_ptr,
                     int                 *ret_val)
{
     D_DEBUG_AT( Fusion_Call, "%s( %p, 0x%x, %d, %p )\n", __FUNCTION__, call, flags, call_arg, call_ptr );

     D_ASSERT( call != NULL );

     if (!call->handler)
          return DR_DESTROYED;

#if D_DEBUG_ENABLED
     if (call->fusion_id == _fusion_id( call->shared ) && direct_log_domain_check( &Fusion_Call ))
          D_DEBUG_AT( Fusion_Call, "  -> %s\n", direct_trace_lookup_symbol_at( call->handler ) );
#endif

     if (!(flags & FCEF_NODIRECT) && call->fusion_id == _fusion_id( call->shared )) {
          int                     ret;
          FusionCallHandlerResult result;

          result = call->handler( call->fusion_id, call_arg, call_ptr, call->ctx, 0, &ret );

          if (result != FCHR_RETURN)
               D_WARN( "local call handler returned FCHR_RETAIN, need FCEF_NODIRECT" );

          if (ret_val)
               *ret_val = ret;
     }
     else {
          FusionCallExecute execute;

          execute.call_id  = call->call_id;
          execute.call_arg = call_arg;
          execute.call_ptr = call_ptr;
          execute.flags    = flags | FCEF_RESUMABLE;
          execute.serial   = 0;

          fusion_world_flush_calls( _fusion_world( call->shared ), 1 );

          while (ioctl( _fusion_fd( call->shared ), FUSION_CALL_EXECUTE, &execute )) {
               switch (errno) {
                    case EINTR:
                         continue;
                    case EINVAL:
                         D_ERROR ("Fusion/Call: invalid call (id 0x%08x)\n", call->call_id);
                         return DR_INVARG;
                    case EIDRM:
                         D_ERROR ("Fusion/Call: call got destroyed (id 0x%08x)\n", call->call_id);
                         return DR_DESTROYED;
                    default:
                         break;
               }

               D_PERROR ("FUSION_CALL_EXECUTE (call id 0x%08x, creator %lu)", call->call_id, call->fusion_id );

               return DR_FAILURE;
          }

          if (ret_val)
               *ret_val = execute.ret_val;
     }

     return DR_OK;
}

DirectResult
fusion_call_execute2(FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *ptr,
                     unsigned int         length,
                     int                 *ret_val)
{
     D_DEBUG_AT( Fusion_Call, "%s( %p, flags 0x%x, arg %d, %p, length %u )\n", __FUNCTION__, call, flags, call_arg, ptr, length );

     D_ASSERT( call != NULL );

//     if (!call->handler)
//          return DR_DESTROYED;

#if D_DEBUG_ENABLED
     if (call->fusion_id == _fusion_id( call->shared ) && direct_log_domain_check( &Fusion_Call ))
          D_DEBUG_AT( Fusion_Call, "  -> %s\n", direct_trace_lookup_symbol_at( call->handler ) );
#endif

     if (!(flags & FCEF_NODIRECT) && call->fusion_id == _fusion_id( call->shared )) {
          int                     ret;
          FusionCallHandlerResult result;

          result = call->handler( call->fusion_id, call_arg, ptr, call->ctx, 0, &ret );

          if (result != FCHR_RETURN)
               D_WARN( "local call handler returned FCHR_RETAIN, need FCEF_NODIRECT" );

          if (ret_val)
               *ret_val = ret;
     }
     else {
          FusionCallExecute2 execute;

          execute.call_id  = call->call_id;
          execute.call_arg = call_arg;
          execute.ptr      = ptr;
          execute.length   = length;
          execute.flags    = flags | FCEF_RESUMABLE;
          execute.serial   = 0;

          fusion_world_flush_calls( _fusion_world( call->shared ), 1 );

          while (ioctl( _fusion_fd( call->shared ), FUSION_CALL_EXECUTE2, &execute )) {
               switch (errno) {
                    case EINTR:
                         continue;
                    case EINVAL:
                         D_ERROR ("Fusion/Call: invalid call (id 0x%08x)\n", call->call_id);
                         return DR_INVARG;
                    case EIDRM:
                         D_ERROR ("Fusion/Call: call got destroyed (id 0x%08x)\n", call->call_id);
                         return DR_DESTROYED;
                    default:
                         break;
               }

               D_PERROR ("FUSION_CALL_EXECUTE2 (call id 0x%08x, creator %lu)", call->call_id, call->fusion_id );

               return DR_FAILURE;
          }

          if (ret_val)
               *ret_val = execute.ret_val;
     }

     return DR_OK;
}

#define FUSION_CALL_RETURN_DATA_MAX          32*1024*1024
#define FUSION_CALL_RETURN_DATA_MAX_ON_STACK 1024

DirectResult
fusion_call_execute3(FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *ptr,
                     unsigned int         length,
                     void                *ret_ptr,
                     unsigned int         ret_size,
                     unsigned int        *ret_length)
{
     FusionWorld *world;
     CallTLS     *call_tls;

     D_DEBUG_AT( Fusion_Call, "%s( %p, flags 0x%x, arg %d, ptr %p, length %u, ret_ptr %p, ret_size %u )\n",
                 __FUNCTION__, call, flags, call_arg, ptr, length, ret_ptr, ret_size );

     if (ret_size > FUSION_CALL_RETURN_DATA_MAX)
          return DR_LIMITEXCEEDED;

     D_ASSERT( call != NULL );

//     if (!call->handler)
//          return DR_DESTROYED;

     world = _fusion_world( call->shared );

#if D_DEBUG_ENABLED
     if (call->fusion_id == fusion_id( world ) && direct_log_domain_check( &Fusion_Call ))
          D_DEBUG_AT( Fusion_Call, "  =-> handler is %s()\n", direct_trace_lookup_symbol_at( call->handler3 ) );
#endif

     call_tls = Call_GetTLS( world );

     if (call->fusion_id == fusion_id( world ) &&
         (!(flags & FCEF_NODIRECT) || (call_tls->dispatcher)))
     {
          FusionCallHandlerResult result;
          unsigned int            execute_length;

          D_ASSERT( call->handler3 != NULL );

          result = call->handler3( call->fusion_id, call_arg, ptr, length, call->ctx, 0, ret_ptr, ret_size, &execute_length );

          if (result != FCHR_RETURN)
               D_WARN( "local call handler returned FCHR_RETAIN, need FCEF_NODIRECT" );

          if (ret_length)
               *ret_length = execute_length;
     }
     else {
          FusionCallExecute3  execute;
          DirectResult        ret   = DR_OK;

          // check whether we can cache this call
          if (flags & FCEF_QUEUE && fusion_config->call_bin_max_num > 0 && length < 10000) {
               D_ASSERT( flags & FCEF_ONEWAY );

               if (call_tls->bins_data_len + length > fusion_config->call_bin_max_data) {
                    ret = fusion_world_flush_calls( world, 0 );
                    if (ret)
                         return ret;
               }

               D_ASSERT( ret_size == 0 );

               call_tls->bins[call_tls->bins_num].call_id    = call->call_id;
               call_tls->bins[call_tls->bins_num].call_arg   = call_arg;
               call_tls->bins[call_tls->bins_num].ptr        = NULL;
               call_tls->bins[call_tls->bins_num].length     = length;
               call_tls->bins[call_tls->bins_num].ret_ptr    = ret_ptr;
               call_tls->bins[call_tls->bins_num].ret_length = ret_size;
               call_tls->bins[call_tls->bins_num].flags      = flags | FCEF_FOLLOW | FCEF_RESUMABLE;
               call_tls->bins[call_tls->bins_num].serial     = 0;

               D_DEBUG_AT( Fusion_Call, "  -> buffered length %u, flags 0x%08x\n", length, call_tls->bins[call_tls->bins_num].flags );

               if (length > 0) {
                    call_tls->bins[call_tls->bins_num].ptr = &call_tls->bins_data[call_tls->bins_data_len];
                    direct_memcpy( &call_tls->bins_data[call_tls->bins_data_len], ptr, length );
                    call_tls->bins_data_len += length;
               }

               call_tls->bins_num++;

               if (call_tls->bins_num == 1)
                    call_tls->bins_create_ts = direct_clock_get_millis();
               else if (call_tls->bins_num >= fusion_config->call_bin_max_num || direct_clock_get_millis() - call_tls->bins_create_ts >= EXECUTE3_BIN_FLUSH_MILLIS)
                    ret = fusion_world_flush_calls( world, 0 );    

               return ret;
          }

          ret = fusion_world_flush_calls( world, 1 );

          execute.call_id    = call->call_id;
          execute.call_arg   = call_arg;
          execute.ptr        = ptr;
          execute.length     = length;
          execute.ret_ptr    = ret_ptr;
          execute.ret_length = ret_size;
          execute.flags      = flags | FCEF_RESUMABLE;
          execute.serial     = 0;

          D_DEBUG_AT( Fusion_Call, "  -> ptr %p, length %u, flags 0x%08x\n", ptr, length, execute.flags );

          D_ASSERT( !(execute.flags & FCEF_ERROR) );

          while (ioctl( world->fusion_fd, FUSION_CALL_EXECUTE3, &execute )) {
               switch (errno) {
                    case EINTR:
                         D_DEBUG_AT( Fusion_Call, "  -> EINTR (flags 0x%08x, serial %u)\n", execute.flags, execute.serial );
                         D_ASSERT( !(execute.flags & FCEF_ERROR) );
                         continue;
                    case EINVAL:
                         D_ERROR ("Fusion/Call: invalid call (id 0x%08x)\n", call->call_id);
                         return DR_INVARG;
                    case EIDRM:
                         D_ERROR ("Fusion/Call: call got destroyed (id 0x%08x)\n", call->call_id);
                         return DR_DESTROYED;
                    default:
                         D_DEBUG_AT( Fusion_Call, "  -> OK (flags 0x%08x)\n", execute.flags );
                         break;
               }

               D_PERROR ("FUSION_CALL_EXECUTE3 (call id 0x%08x, creator %lu)", call->call_id, call->fusion_id );

               return DR_FAILURE;
          }

          if (ret_length)
               *ret_length = execute.ret_length;
     }

     return DR_OK;
}

DirectResult
fusion_world_flush_calls( FusionWorld *world, int lock )
{
     DirectResult  ret = DR_OK;
     CallTLS      *call_tls;

     call_tls = Call_GetTLS( world );

     D_DEBUG_AT( Fusion_Call, "%s( %p, lock %d )\n", __FUNCTION__, world, lock );

     if (call_tls->bins_num > 0) {
#if D_DEBUG_ENABLED
          D_DEBUG_AT( Fusion_Call, "  -> num %d, length %u\n", call_tls->bins_num, call_tls->bins_data_len );

          if (direct_log_domain_check( &Fusion_Call )) {
               int i;

               for (i=0; i<call_tls->bins_num; i++) {
                    D_DEBUG_AT( Fusion_Call, "  -> [%2d] call_id 0x%08x, call_arg %3d, length %3u, flasg 0x%08x\n", i,
                                call_tls->bins[i].call_id,
                                call_tls->bins[i].call_arg,
                                call_tls->bins[i].length,
                                call_tls->bins[i].flags );
               }
          }
#endif

          if (call_tls->dispatcher) {
               D_DEBUG_AT( Fusion_Call, "  -> I AM THE DISPATCHER, NOT FLUSHING\n" );
               D_WARN("no flush in dispatcher");
               return DR_OK;
          }

          call_tls->bins[call_tls->bins_num - 1].flags &= ~(FCEF_FOLLOW | FCEF_QUEUE);

          while (ioctl( world->fusion_fd, FUSION_CALL_EXECUTE3, call_tls->bins )) {
               switch (errno) {
                    case EINTR:
                         continue;
                    case EINVAL:
                         D_ERROR ("Fusion/Call: invalid call (id 0x%08x)\n", call_tls->bins[0].call_id);
                         ret = DR_INVARG;
                         break;
                    case EIDRM:
                         D_ERROR ("Fusion/Call: call got destroyed (id 0x%08x)\n", call_tls->bins[0].call_id);
                         ret = DR_DESTROYED;
                         break;
                    default:
                         break;
               }

               D_PERROR ("FUSION_CALL_EXECUTE3 (num %d, len %d)", call_tls->bins_num, call_tls->bins_data_len );

               ret = DR_FAILURE;
               break;
          }

          call_tls->bins_num      = 0;
          call_tls->bins_data_len = 0;
     }

     return ret;
}

DirectResult
fusion_call_return( FusionCall   *call,
                    unsigned int  serial,
                    int           val )
{
     FusionCallReturn call_ret;

     D_DEBUG_AT( Fusion_Call, "%s( %p, %u, %d )\n", __FUNCTION__, call, serial, val );

     D_ASSERT( call != NULL );

     if (direct_log_domain_check( &Fusion_Call )) // avoid call to direct_trace_lookup_symbol_at
          D_DEBUG_AT( Fusion_Call, "  -> %s\n", direct_trace_lookup_symbol_at( call->handler ) );

     D_ASSUME( serial != 0 );
     if (!serial)
          return DR_UNSUPPORTED;

     call_ret.call_id = call->call_id;
     call_ret.val     = val;
     call_ret.serial  = serial;

     fusion_world_flush_calls( _fusion_world( call->shared ), 1 );

     while (ioctl (_fusion_fd( call->shared ), FUSION_CALL_RETURN, &call_ret)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EIDRM:
                    D_WARN( "caller withdrawn (signal?)" );
                    return DR_NOCONTEXT;
               case EINVAL:
                    D_ERROR( "Fusion/Call: invalid call\n" );
                    return DR_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_RETURN");

          return DR_FAILURE;
     }

     return DR_OK;
}

DirectResult
fusion_call_return3( FusionCall   *call,
                     unsigned int  serial,
                     void         *ptr,
                     unsigned int  length )
{
     FusionCallReturn3 call_ret;

     D_DEBUG_AT( Fusion_Call, "%s( %p, serial %u, ptr %p, length %u )\n", __FUNCTION__, call, serial, ptr, length );

     D_ASSERT( call != NULL );

     if (direct_log_domain_check( &Fusion_Call )) // avoid call to direct_trace_lookup_symbol_at
          D_DEBUG_AT( Fusion_Call, "  -> %s\n", direct_trace_lookup_symbol_at( call->handler ) );

     D_ASSUME( serial != 0 );
     if (!serial)
          return DR_UNSUPPORTED;

     call_ret.call_id = call->call_id;
     call_ret.serial  = serial;
     call_ret.ptr     = ptr;
     call_ret.length  = length;

     fusion_world_flush_calls( _fusion_world( call->shared ), 1 );

     while (ioctl (_fusion_fd( call->shared ), FUSION_CALL_RETURN3, &call_ret)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EIDRM:
                    D_WARN( "caller withdrawn (signal?)" );
                    return DR_NOCONTEXT;
               case EINVAL:
                    D_ERROR( "Fusion/Call: invalid call\n" );
                    return DR_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_RETURN3");

          return DR_FAILURE;
     }

     return DR_OK;
}

DirectResult
fusion_call_get_owner( FusionCall *call,
                       FusionID   *ret_fusion_id )
{
     FusionCallGetOwner get_owner;

     D_DEBUG_AT( Fusion_Call, "%s( %p )\n", __FUNCTION__, call );

     D_ASSERT( call != NULL );
     D_ASSERT( ret_fusion_id != NULL );

     get_owner.call_id = call->call_id;

     while (ioctl (_fusion_fd( call->shared ), FUSION_CALL_GET_OWNER, &get_owner)) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_GET_OWNER");

          return DR_FAILURE;
     }

     *ret_fusion_id = get_owner.fusion_id;

     return DR_OK;
}

DirectResult
fusion_call_set_quota( FusionCall   *call,
                       FusionID      fusion_id,
                       unsigned int  limit )
{
     FusionCallSetQuota set_quota;

     D_DEBUG_AT( Fusion_Call, "%s( %p, fusion_id %lu, limit %u )\n", __FUNCTION__, call, fusion_id, limit );

     D_ASSERT( call != NULL );

     set_quota.call_id   = call->call_id;
     set_quota.fusion_id = fusion_id;
     set_quota.limit     = limit;

     while (ioctl (_fusion_fd( call->shared ), FUSION_CALL_SET_QUOTA, &set_quota)) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_SET_QUOTA");

          return DR_FAILURE;
     }

     return DR_OK;
}

DirectResult
fusion_call_destroy (FusionCall *call)
{
     D_DEBUG_AT( Fusion_Call, "%s( %p )\n", __FUNCTION__, call );

     D_ASSERT( call != NULL );
     D_ASSERT( call->handler != NULL || call->handler3 != NULL );

     D_DEBUG_AT( Fusion_Call, "  -> call_id 0x%08x\n", call->call_id );

     if (direct_log_domain_check( &Fusion_Call )) // avoid call to direct_trace_lookup_symbol_at
          D_DEBUG_AT( Fusion_Call, "  -> %s\n", direct_trace_lookup_symbol_at( call->handler ? call->handler :
                                                                               (FusionCallHandler) call->handler3 ) );

     while (ioctl (_fusion_fd( call->shared ), FUSION_CALL_DESTROY, &call->call_id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
//FIXME: kernel module destroys calls from exiting process already                    D_ERROR ("Fusion/Call: invalid call\n");
                    return DR_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_CALL_DESTROY");

          return DR_FAILURE;
     }

     call->handler = NULL;

     return DR_OK;
}

DirectResult
fusion_call_add_permissions( FusionCall            *call,
                             FusionID               fusion_id,
                             FusionCallPermissions  call_permissions )
{
     FusionEntryPermissions permissions;

     permissions.type        = FT_CALL;
     permissions.id          = call->call_id;
     permissions.fusion_id   = fusion_id;
     permissions.permissions = 0;

     if (call_permissions & FUSION_CALL_PERMIT_EXECUTE) {
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_CALL_EXECUTE );
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_CALL_EXECUTE2 );
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_CALL_EXECUTE3 );
     }

     while (ioctl( _fusion_fd( call->shared ), FUSION_ENTRY_ADD_PERMISSIONS, &permissions ) < 0) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/Call: FUSION_ENTRY_ADD_PERMISSIONS( id %d ) failed!\n", call->call_id );
               return DR_FAILURE;
          }
     }

     return DR_OK;
}

void
_fusion_call_process( FusionWorld *world, int call_id, FusionCallMessage *msg, void *ptr )
{
     FusionCallHandlerResult result = FCHR_RETURN;
     FusionCallHandler       call_handler;
     FusionCallReturn        call_ret = {
          .val = 0
     };


     D_DEBUG_AT( Fusion_Call, "%s( call_id %d, msg %p, ptr %p)\n", __FUNCTION__, call_id, msg, ptr );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_ASSERT( msg != NULL );
     D_ASSERT( msg->handler != NULL );

     call_handler = msg->handler;

     if (direct_log_domain_check( &Fusion_Call )) // avoid call to direct_trace_lookup_symbol_at
          D_DEBUG_AT( Fusion_Call, "  -> %s\n", direct_trace_lookup_symbol_at( call_handler ) );

     result = call_handler( msg->caller, msg->call_arg, ptr ? ptr : msg->call_ptr, msg->ctx, msg->serial, &call_ret.val );

     switch (result) {
          case FCHR_RETURN:
               if (msg->serial) {
                    call_ret.serial  = msg->serial;
                    call_ret.call_id = call_id;

                    while (ioctl (world->fusion_fd, FUSION_CALL_RETURN, &call_ret)) {
                         switch (errno) {
                              case EINTR:
                                   continue;
                              case EIDRM:
                                   D_WARN( "caller withdrawn (signal?)" );
                                   return;
                              case EINVAL:
                                   D_ERROR( "Fusion/Call: invalid call\n" );
                                   return;
                              default:
                                   D_PERROR( "FUSION_CALL_RETURN" );
                                   return;
                         }
                    }
               }
               break;

          case FCHR_RETAIN:
               break;

          default:
               D_BUG( "unknown result %d from call handler", result );
     }
}

void
_fusion_call_process3( FusionWorld *world, int call_id, FusionCallMessage3 *msg, void *ptr )
{
     FusionCallHandlerResult  result = FCHR_RETURN;
     FusionCallHandler3       call_handler;
     FusionCallReturn3        call_ret;
     char                    *ret_ptr    = NULL;
     unsigned int             ret_length = 0;

     D_DEBUG_AT( Fusion_Call, "%s( call_id %d, msg %p, ptr %p)\n", __FUNCTION__, call_id, msg, ptr );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_ASSERT( msg != NULL );
     D_ASSERT( msg->handler != NULL );

     call_handler = msg->handler;

     if (direct_log_domain_check( &Fusion_Call )) // avoid call to direct_trace_lookup_symbol_at
          D_DEBUG_AT( Fusion_Call, "  -> %s\n", direct_trace_lookup_symbol_at( call_handler ) );

     if (msg->ret_length > FUSION_CALL_RETURN_DATA_MAX) {
          D_ERROR( "Fusion/Call: Maximum return data length (%u) exceeded (%u)!\n", FUSION_CALL_RETURN_DATA_MAX, msg->ret_length );
     }
     else {
          if (msg->ret_length > FUSION_CALL_RETURN_DATA_MAX_ON_STACK) {
               ret_ptr = D_MALLOC( msg->ret_length );
               if (!ret_ptr)
                    D_OOM();
          }
          else
               ret_ptr = alloca( msg->ret_length );
     }

     if (ret_ptr)
          result = call_handler( msg->caller, msg->call_arg, ptr ? ptr : msg->call_ptr, msg->call_length, msg->ctx, msg->serial, ret_ptr, msg->ret_length, &ret_length );

     switch (result) {
          case FCHR_RETURN:
               if (msg->serial) {
                    call_ret.call_id = call_id;
                    call_ret.serial  = msg->serial;
                    call_ret.ptr     = ret_ptr;
                    call_ret.length  = ret_length;

                    while (ioctl (world->fusion_fd, FUSION_CALL_RETURN3, &call_ret)) {
                         switch (errno) {
                              case EINTR:
                                   continue;
                              case EIDRM:
                                   D_DEBUG_AT( Fusion_Call, "  -> caller withdrawn (signal?)\n" );
                                   goto out;
                              case EINVAL:
                                   D_ERROR( "Fusion/Call: invalid call\n" );
                                   goto out;
                              default:
                                   D_PERROR( "FUSION_CALL_RETURN3" );
                                   goto out;
                         }
                    }
               }
               break;

          case FCHR_RETAIN:
               break;

          default:
               D_BUG( "unknown result %d from call handler", result );
     }

out:
     if (msg->ret_length > FUSION_CALL_RETURN_DATA_MAX_ON_STACK)
          D_FREE( ret_ptr );
}

#else /* FUSION_BUILD_KERNEL */

#include <fcntl.h>
#include <unistd.h>


typedef struct {
     int       call_id;
     FusionID  fusion_id;

     void     *handler;
     void     *handler3;
     void     *ctx;
} CallInfo;

DirectResult
fusion_call_init (FusionCall        *call,
                  FusionCallHandler  handler,
                  void              *ctx,
                  const FusionWorld *world)
{
     D_ASSERT( call != NULL );
     D_ASSERT( handler != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     memset( call, 0, sizeof(FusionCall) );

     call->call_id = ++world->shared->call_ids;

     /* Store handler, called directly when called by ourself. */
     call->handler = handler;
     call->ctx     = ctx;

     /* Store own fusion id. */
     call->fusion_id = fusion_id( world );

     /* Keep back pointer to shared world data. */
     call->shared = world->shared;


     CallInfo *info = SHCALLOC( world->shared->main_pool, 1, sizeof(CallInfo) );

     D_ASSERT( info != NULL );

     info->call_id   = call->call_id;
     info->fusion_id = call->fusion_id;
     info->handler   = call->handler;
     info->handler3  = call->handler3;
     info->ctx       = call->ctx;

     //D_INFO_LINE_MSG("call init %d owner %lu, me %lu\n",call->call_id,call->fusion_id, fusion_id( world ));

     fusion_hash_insert( world->shared->call_hash, (void*)(long) call->call_id, info );

     return DR_OK;
}

DirectResult
fusion_call_init3 (FusionCall         *call,
                   FusionCallHandler3  handler3,
                   void               *ctx,
                   const FusionWorld  *world)
{
     D_ASSERT( call != NULL );
     D_ASSERT( handler3 != NULL );

     memset( call, 0, sizeof(FusionCall) );

     call->call_id = ++world->shared->call_ids;

     /* Store handler, called directly when called by ourself. */
     call->handler3 = handler3;
     call->ctx      = ctx;

     /* Store own fusion id. */
     call->fusion_id = fusion_id( world );

     /* Keep back pointer to shared world data. */
     call->shared = world->shared;


     CallInfo *info = SHCALLOC( world->shared->main_pool, 1, sizeof(CallInfo) );

     D_ASSERT( info != NULL );

     info->call_id   = call->call_id;
     info->fusion_id = call->fusion_id;
     info->handler   = call->handler;
     info->handler3  = call->handler3;
     info->ctx       = call->ctx;

     //D_INFO_LINE_MSG("call init %d owner %lu, me %lu\n",call->call_id,call->fusion_id, fusion_id( world ));

     fusion_hash_insert( world->shared->call_hash, (void*)(long) call->call_id, info );

     return DR_OK;
}

DirectResult
fusion_call_init_from( FusionCall        *call,
                       int                call_id,
                       const FusionWorld *world )
{
     D_DEBUG_AT( Fusion_Call, "%s( %p, %d, %p )\n", __FUNCTION__, call, call_id, world );

     D_ASSERT( call != NULL );
     D_ASSERT( call_id != 0 );
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     CallInfo *info = fusion_hash_lookup( world->shared->call_hash, (void*)(long) call_id );

     D_ASSERT( info != NULL );
     D_ASSERT( info->call_id == call_id );

     memset( call, 0, sizeof(FusionCall) );

     /* Store call id. */
     call->call_id = call_id;

     /* Store handlers, called directly when called by ourself. */
     call->handler  = info->handler;
     call->handler3 = info->handler3;
     call->ctx      = info->ctx;

     /* Store own fusion id. */
     call->fusion_id = info->fusion_id;

     /* Keep back pointer to shared world data. */
     call->shared = world->shared;

     //D_INFO_LINE_MSG("call init %d owner %lu, me %lu\n",call->call_id,call->fusion_id, fusion_id( world ));

     D_DEBUG_AT( Fusion_Call, "  -> call id %d\n", call->call_id );

     return DR_OK;
}

DirectResult
fusion_call_set_name( FusionCall *call,
                      const char *name )
{
     D_ASSERT( call != NULL );
     D_ASSERT( name != NULL );

     return DR_OK;
}

static DirectResult
fusion_call_execute_internal (FusionCall          *call,
                              FusionCallExecFlags  flags,
                              int                  call_arg,
                              void                *call_ptr,
                              unsigned int         length,
                              void                *ret_ptr,
                              unsigned int         ret_size,
                              unsigned int        *ret_length)
{
     DirectResult        ret = DR_OK;
     FusionWorld        *world;
     struct sockaddr_un  addr;
     

     char               msg_buf[sizeof(FusionCallMessage) + length];
     FusionCallMessage *msg = (FusionCallMessage *) msg_buf;


     D_ASSERT( call != NULL );

     if (!call->handler && !call->handler3)
          return DR_DESTROYED;

     world = _fusion_world( call->shared );

     //D_INFO_LINE_MSG("call execute %d owner %lu, me %lu\n",call->call_id,call->fusion_id, _fusion_id( call->shared ));
     if (call->fusion_id == fusion_id( world ) &&
         (!(flags & FCEF_NODIRECT) || (call->handler3 && (direct_thread_self() == world->dispatch_loop))))
     {
          FusionCallHandlerResult result;

          if (call->handler) {
               D_ASSERT( length == sizeof(void*) );
               result = call->handler( _fusion_id( call->shared ), call_arg, *(void**)call_ptr, call->ctx, 0, ret_ptr );
          }
          else {
               D_ASSERT( call->handler3 != NULL );

               result = call->handler3( _fusion_id( call->shared ), call_arg, call_ptr, length, call->ctx, 0, ret_ptr, ret_size, ret_length );
          }

          if (result != FCHR_RETURN)
               D_WARN( "local call handler returned FCHR_RETAIN, need FCEF_NODIRECT" );
               
          return DR_OK;
     }
     
     msg->type        = FMT_CALL;
     msg->caller      = world->fusion_id;
     msg->call_id     = call->call_id;
     msg->call_arg    = call_arg;
     msg->call_length = length;
     msg->ret_length  = ret_size;
     msg->handler     = call->handler;
     msg->handler3    = call->handler3;
     msg->ctx         = call->ctx;
     msg->flags       = flags;

     direct_memcpy( msg + 1, call_ptr, length );
     
     if (flags & FCEF_ONEWAY) {
          /* Invalidate serial. */
          msg->serial = -1;
          
          /* Send message. */
          addr.sun_family = AF_UNIX;
          snprintf( addr.sun_path, sizeof(addr.sun_path), 
                    "/tmp/.fusion-%d/%lx", call->shared->world_index, call->fusion_id );

          ret = _fusion_send_message( world->fusion_fd, msg, sizeof(FusionCallMessage) + length, &addr );
     }
     else {
          int       fd;
          socklen_t len;
          int       err;

          fd = socket( PF_LOCAL, SOCK_RAW, 0 );
          if (fd < 0) {
               D_PERROR( "Fusion/Call: Error creating local socket!\n" ) ;
               return DR_IO;
          }

          /* Set close-on-exec flag. */
          fcntl( fd, F_SETFD, FD_CLOEXEC );
          
          addr.sun_family = AF_UNIX;
          len = snprintf( addr.sun_path, sizeof(addr.sun_path), 
                          "/tmp/.fusion-%d/call.%x.", fusion_world_index( world ), call->call_id ); 
          
          /* Generate call serial (socket address is based on it). */
          for (msg->serial = 0; msg->serial <= 0xffffff; msg->serial++) {
               snprintf( addr.sun_path+len, sizeof(addr.sun_path)-len, "%x", msg->serial );
               err = bind( fd, (struct sockaddr*)&addr, sizeof(addr) );
               if (err == 0) {
                    chmod( addr.sun_path, 0660 );
                    /* Change group, if requested. */
                    if (fusion_config->shmfile_gid != (gid_t)-1)
                         chown( addr.sun_path, -1, fusion_config->shmfile_gid );
                    break;
               }
          }
          
          if (err < 0) {
               D_PERROR( "Fusion/Call: Error binding local socket!\n" );
               close( fd );
               return DR_IO;
          }

          /* Send message. */
          snprintf( addr.sun_path, sizeof(addr.sun_path), 
                    "/tmp/.fusion-%d/%lx", call->shared->world_index, call->fusion_id );

          ret = _fusion_send_message( fd, msg, sizeof(FusionCallMessage) + length, &addr );
          if (ret == DR_OK) {
               char              buf[sizeof(FusionCallReturn) + ret_size];
               FusionCallReturn *callret = (FusionCallReturn *) buf;

               /* Wait for reply. */
               ret = _fusion_recv_message( fd, buf, sizeof(FusionCallReturn) + ret_size, NULL );
               if (ret == DR_OK) {
                    D_ASSERT( callret->length <= ret_size );

                    if (callret->length) {
                         D_ASSERT( ret_ptr != NULL );

                         direct_memcpy( ret_ptr, callret + 1, callret->length );
                    }

                    if (ret_length)
                         *ret_length = callret->length;
               } 
          }
          
          len = sizeof(addr);
          if (getsockname( fd, (struct sockaddr*)&addr, &len ) == 0)
               unlink( addr.sun_path );
          close( fd );
     }

     return ret;
}

DirectResult
fusion_call_execute (FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *call_ptr,
                     int                 *ret_val)
{
     return fusion_call_execute_internal( call, flags, call_arg, &call_ptr, sizeof(call_ptr), ret_val, sizeof(*ret_val), NULL );
}

DirectResult
fusion_call_execute2(FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *call_ptr,
                     unsigned int         length,
                     int                 *ret_val)
{
     return fusion_call_execute_internal( call, flags, call_arg, call_ptr, length, ret_val, 4, NULL );
}

DirectResult
fusion_call_execute3(FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *call_ptr,
                     unsigned int         length,
                     void                *ret_ptr,
                     unsigned int         ret_size,
                     unsigned int        *ret_length)
{
     return fusion_call_execute_internal( call, flags, call_arg, call_ptr, length, ret_ptr, ret_size, ret_length );
}

static DirectResult
fusion_call_return_internal( FusionCall   *call,
                             unsigned int  serial,
                             const void   *ptr,
                             unsigned int  length )
{
     struct sockaddr_un addr;

     char              buf[sizeof(FusionCallReturn) + length];
     FusionCallReturn *callret = (FusionCallReturn *) buf;

     D_ASSERT( call != NULL );

     addr.sun_family = AF_UNIX;
     snprintf( addr.sun_path, sizeof(addr.sun_path), 
               "/tmp/.fusion-%d/call.%x.%x", call->shared->world_index, call->call_id, serial );

     callret->type   = FMT_CALLRET;
     callret->length = length;

     if (length) {
          D_ASSERT( ptr != NULL );

          direct_memcpy( callret + 1, ptr, length );
     }

     return _fusion_send_message( _fusion_fd( call->shared ), callret, sizeof(FusionCallReturn) + length, &addr );
}

DirectResult
fusion_call_return( FusionCall   *call,
                    unsigned int  serial,
                    int           val )
{
     return fusion_call_return_internal( call, serial, &val, sizeof(int) );
}

DirectResult
fusion_call_return3( FusionCall   *call,
                     unsigned int  serial,
                     void         *ptr,
                     unsigned int  length )
{
     return fusion_call_return_internal( call, serial, ptr, length );
}

DirectResult
fusion_call_get_owner( FusionCall *call,
                       FusionID   *ret_fusion_id )
{
     D_DEBUG_AT( Fusion_Call, "%s( %p )\n", __FUNCTION__, call );

     D_ASSERT( call != NULL );
     D_ASSERT( ret_fusion_id != NULL );

     // FIXME: should return actual owner
     *ret_fusion_id = call->fusion_id;

     return DR_OK;
}

DirectResult
fusion_call_set_quota( FusionCall   *call,
                       FusionID      fusion_id,
                       unsigned int  limit )
{
     D_DEBUG_AT( Fusion_Call, "%s( %p, fusion_id %lu, limit %u )\n", __FUNCTION__, call, fusion_id, limit );

     D_ASSERT( call != NULL );

     return DR_OK;
}

DirectResult
fusion_call_destroy (FusionCall *call)
{
     D_ASSERT( call != NULL );
     D_ASSERT( call->handler != NULL || call->handler3 != NULL );

     CallInfo *info = fusion_hash_lookup( call->shared->call_hash, (void*)(long) call->call_id );

     D_ASSERT( info != NULL );
     D_ASSERT( info->call_id == call->call_id );

     fusion_hash_remove( call->shared->call_hash, (void*)(long) call->call_id, NULL, NULL );

     SHFREE( call->shared->main_pool, info );

     call->handler  = NULL;
     call->handler3 = NULL;

     return DR_OK;
}

DirectResult
fusion_call_add_permissions( FusionCall            *call,
                             FusionID               fusion_id,
                             FusionCallPermissions  call_permissions )
{
     D_UNIMPLEMENTED();

     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_world_flush_calls( FusionWorld *world, int lock )
{
     return DR_OK;
}

void
_fusion_call_process( FusionWorld *world, int call_id, FusionCallMessage *msg, void *ptr )
{
     FusionCallHandlerResult result;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_ASSERT( msg != NULL );

     char              buf[sizeof(FusionCallReturn) + msg->ret_length];
     FusionCallReturn *callret = (FusionCallReturn *) buf;

     if (msg->handler) {
          FusionCallHandler call_handler = msg->handler;

          D_ASSERT( call_handler != NULL );

          callret->type   = FMT_CALLRET;
          callret->length = sizeof(int);

          D_ASSERT( msg->call_length == sizeof(void*) );

          result = call_handler( msg->caller, msg->call_arg, ptr, msg->ctx, msg->serial, (int*)(callret + 1) );
          switch (result) {
               case FCHR_RETURN:
                    if (!(msg->flags & FCEF_ONEWAY)) {
                         struct sockaddr_un addr;

                         addr.sun_family = AF_UNIX;
                         snprintf( addr.sun_path, sizeof(addr.sun_path),
                                   "/tmp/.fusion-%d/call.%x.%x", fusion_world_index( world ), call_id, msg->serial );

                         if (_fusion_send_message( world->fusion_fd, callret, sizeof(FusionCallMessage) + callret->length, &addr ))
                              D_ERROR( "Fusion/Call: Couldn't send call return (serial: 0x%08x)!\n", msg->serial );
                    }
                    break;

               case FCHR_RETAIN:
                    break;

               default:
                    D_BUG( "unknown result %d from call handler", result );
                    break;
          }
     }
     else {
          FusionCallHandler3 call_handler3 = msg->handler3;

          D_ASSERT( call_handler3 != NULL );

          callret->type   = FMT_CALLRET;
          callret->length = 0;

          result = call_handler3( msg->caller, msg->call_arg, ptr, msg->call_length, msg->ctx, msg->serial, callret + 1, msg->ret_length, &callret->length );
          switch (result) {
               case FCHR_RETURN:
                    if (!(msg->flags & FCEF_ONEWAY)) {
                         struct sockaddr_un addr;

                         addr.sun_family = AF_UNIX;
                         snprintf( addr.sun_path, sizeof(addr.sun_path),
                                   "/tmp/.fusion-%d/call.%x.%x", fusion_world_index( world ), call_id, msg->serial );

                         if (_fusion_send_message( world->fusion_fd, callret, sizeof(FusionCallMessage) + callret->length, &addr ))
                              D_ERROR( "Fusion/Call: Couldn't send call return (serial: 0x%08x)!\n", msg->serial );
                    }
                    break;

               case FCHR_RETAIN:
                    break;

               default:
                    D_BUG( "unknown result %d from call handler", result );
                    break;
          }
     }
}

void
__Fusion_call_init( void )
{
}

void
__Fusion_call_deinit( void )
{
}

#endif /* FUSION_BUILD_KERNEL */

#else  /* FUSION_BUILD_MULTI */

DirectResult
fusion_call_init (FusionCall        *call,
                  FusionCallHandler  handler,
                  void              *ctx,
                  const FusionWorld *world)
{
     D_ASSERT( call != NULL );
     D_ASSERT( handler != NULL );

     /* Called locally. */
     call->handler = handler;
     call->ctx     = ctx;

     call->shared = world->shared;

     return DR_OK;
}

DirectResult
fusion_call_init3 (FusionCall         *call,
                   FusionCallHandler3  handler3,
                   void               *ctx,
                   const FusionWorld  *world)
{
     D_ASSERT( call != NULL );
     D_ASSERT( handler3 != NULL );

     /* Called locally. */
     call->handler3 = handler3;
     call->ctx      = ctx;

     call->shared = world->shared;

     return DR_OK;
}

DirectResult
fusion_call_init_from( FusionCall        *call,
                       int                call_id,
                       const FusionWorld *world )
{
     D_DEBUG_AT( Fusion_Call, "%s( %p, %d, %p )\n", __FUNCTION__, call, call_id, world );

     D_ASSERT( call != NULL );
     D_ASSERT( call_id != 0 );
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     memset( call, 0, sizeof(FusionCall) );

     /* Store call id. */
     call->call_id = call_id;

     /* Keep back pointer to shared world data. */
     call->shared = world->shared;

     D_DEBUG_AT( Fusion_Call, "  -> call id %d\n", call->call_id );

     return DR_OK;
}

DirectResult
fusion_call_set_name( FusionCall *call,
                      const char *name )
{
     D_ASSERT( call != NULL );
     D_ASSERT( name != NULL );

     return DR_OK;
}

DirectResult
fusion_call_execute (FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *call_ptr,
                     int                 *ret_val)
{
     FusionCallHandlerResult    ret;
     FusionEventDispatcherCall  msg;
     FusionEventDispatcherCall *ret_msg = &msg;

     D_ASSERT( call != NULL );

     if (!call->handler)
          return DR_DESTROYED;

     if (!(flags & FCEF_NODIRECT) || direct_thread_self() == call->shared->world->event_dispatcher_thread)
          return call->handler( 1, call_arg, call_ptr, call->ctx, 0, ret_val );

     msg.processed = 0;
     msg.reaction = 0;
     msg.call_handler = call->handler;
     msg.call_handler3 = 0;
     msg.call_ctx = call->ctx;
     msg.flags = flags;
     msg.call_arg = call_arg;
     msg.ptr = call_ptr;
     msg.length = 0;
     msg.ret_val = 0;
     msg.ret_ptr = 0;
     msg.ret_size = 0;
     msg.ret_length = 0;

     ret = _fusion_event_dispatcher_process( call->shared->world, &msg, &ret_msg );

     if (!(flags & FCEF_ONEWAY) && ret_val)
          *ret_val = ret_msg->ret_val;

     return ret;
}

DirectResult
fusion_call_execute2(FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *ptr,
                     unsigned int         length,
                     int                 *ret_val)
{
     DirectResult               ret;
     FusionEventDispatcherCall  msg;
     FusionEventDispatcherCall *ret_msg = &msg;

     D_ASSERT( call != NULL );

     if (!call->handler)
          return DR_DESTROYED;

     if (!(flags & FCEF_NODIRECT) || direct_thread_self() == call->shared->world->event_dispatcher_thread)
          return call->handler( 1, call_arg, ptr, call->ctx, 0, ret_val );

     msg.processed = 0;
     msg.reaction = 0;
     msg.call_handler = call->handler;
     msg.call_handler3 = 0;
     msg.call_ctx = call->ctx;
     msg.flags = flags;
     msg.call_arg = call_arg;
     msg.ptr = ptr;
     msg.length = length;
     msg.ret_val = 0;
     msg.ret_ptr = 0;
     msg.ret_size = 0;
     msg.ret_length = 0;

     ret = _fusion_event_dispatcher_process( call->shared->world, &msg, &ret_msg );
     if (!(flags & FCEF_ONEWAY) && ret_val)
         *ret_val = ret_msg->ret_val;

     return ret;
}

DirectResult
fusion_call_execute3(FusionCall          *call,
                     FusionCallExecFlags  flags,
                     int                  call_arg,
                     void                *ptr,
                     unsigned int         length,
                     void                *ret_ptr,
                     unsigned int         ret_size,
                     unsigned int        *ret_length)
{
     FusionCallHandlerResult    ret;
     FusionEventDispatcherCall  msg;
     FusionEventDispatcherCall *ret_msg = &msg;

     D_ASSERT( call != NULL );

     if (!call->handler3)
          return DR_DESTROYED;

     if (!(flags & FCEF_NODIRECT) || direct_thread_self() == call->shared->world->event_dispatcher_thread) {
          unsigned int ret_len;

          ret = call->handler3( 1, call_arg, ptr, length, call->ctx, 0, ret_ptr, ret_size, &ret_len );

          if (ret_length)
               *ret_length = ret_len;

          return ret;
     }

     msg.processed = 0;
     msg.reaction = 0;
     msg.call_handler = 0;
     msg.call_handler3 = call->handler3;
     msg.call_ctx = call->ctx;
     msg.flags = flags;
     msg.call_arg = call_arg;
     msg.ptr = ptr;
     msg.length = length;
     msg.ret_val = 0;
     msg.ret_ptr = ret_ptr;
     msg.ret_size = ret_size;
     msg.ret_length = 0;

     ret = _fusion_event_dispatcher_process( call->shared->world, &msg, &ret_msg );

     if (!(flags & FCEF_ONEWAY) && ret_length)
          *ret_length = ret_msg->ret_length;

     return ret;
}

DirectResult
fusion_call_return( FusionCall   *call,
                    unsigned int  serial,
                    int           val )
{
     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_call_get_owner( FusionCall *call,
                       FusionID   *ret_fusion_id )
{
     *ret_fusion_id = FUSION_ID_MASTER;

     return DR_OK;
}

DirectResult
fusion_call_destroy (FusionCall *call)
{
     D_ASSERT( call != NULL );
     D_ASSERT( call->handler != NULL || call->handler3 != NULL );

     call->handler  = NULL;
     call->handler3 = NULL;

     return DR_OK;
}

DirectResult
fusion_call_add_permissions( FusionCall            *call,
                             FusionID               fusion_id,
                             FusionCallPermissions  call_permissions )
{
     return DR_OK;
}

DirectResult
fusion_call_set_quota( FusionCall   *call,
                       FusionID      fusion_id,
                       unsigned int  limit )
{
     return DR_OK;
}

DirectResult
fusion_world_flush_calls( FusionWorld *world, int lock )
{
     return DR_OK;
}

void
__Fusion_call_init( void )
{
}

void
__Fusion_call_deinit( void )
{
}

#endif

