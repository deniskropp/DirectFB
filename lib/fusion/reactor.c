/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <pthread.h>

#include <fusion/build.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/util.h>

#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/shmalloc.h>
#include <fusion/reactor.h>

#include "fusion_internal.h"


#if FUSION_BUILD_MULTI

D_DEBUG_DOMAIN( Fusion_Reactor, "Fusion/Reactor", "Fusion's Reactor" );

struct __Fusion_FusionReactor {
     int                magic;

     int                id;        /* reactor id                          */
     int                msg_size;  /* size of each message                */
     bool               direct;
     bool               destroyed;

     DirectLink        *globals;
     FusionSkirmish    *globals_lock;

     FusionWorldShared *shared;
     
#if !FUSION_BUILD_KERNEL
     DirectLink        *listeners;  /* list of attached listeners */
     FusionSkirmish     listeners_lock;
#endif
};

typedef struct {
     DirectLink         link;

     int                magic;

     pthread_rwlock_t   lock;

     int                reactor_id;
     FusionReactor     *reactor;

     DirectLink        *links; /* reactor listeners attached to node  */
} ReactorNode;

typedef struct {
     DirectLink         link;

     int                magic;

     Reaction          *reaction;
     int                channel;
} NodeLink;

/**************************************************************************************************/

static ReactorNode *lock_node      ( int                 reactor_id,
                                     bool                add_it,
                                     bool                wlock,
                                     FusionReactor      *reactor, /* one of reactor and world must not be NULL */
                                     FusionWorld        *world );

static void         unlock_node    ( ReactorNode        *node );

static void         process_globals( FusionReactor      *reactor,
                                     const void         *msg_data,
                                     const ReactionFunc *globals );

/**************************************************************************************************/

#if FUSION_BUILD_KERNEL

FusionReactor *
fusion_reactor_new( int                msg_size,
                    const char        *name,
                    const FusionWorld *world )
{
     FusionEntryInfo    info;
     FusionReactor     *reactor;
     FusionWorldShared *shared;

     D_ASSERT( msg_size > 0 );
     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );

     shared = world->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "fusion_reactor_new( '%s', size %d )\n", name ? : "", msg_size );

     /* allocate shared reactor data */
     reactor = SHCALLOC( shared->main_pool, 1, sizeof (FusionReactor) );
     if (!reactor) {
          D_OOSHM();
          return NULL;
     }

     /* create a new reactor */
     while (ioctl( world->fusion_fd, FUSION_REACTOR_NEW, &reactor->id )) {
          if (errno == EINTR)
              continue;

          D_PERROR( "FUSION_REACTOR_NEW" );
          SHFREE( shared->main_pool, reactor );
          return NULL;
     }

     /* set the static message size, should we make dynamic? (TODO?) */
     reactor->msg_size = msg_size;
     
     /* Set default lock for global reactions. */
     reactor->globals_lock = &shared->reactor_globals;

     D_DEBUG_AT( Fusion_Reactor, "  -> new reactor %p [%d] with lock %p [%d]\n",
                 reactor, reactor->id, reactor->globals_lock, reactor->globals_lock->multi.id );

     reactor->shared = shared;
     reactor->direct = true;

     D_MAGIC_SET( reactor, FusionReactor );


     info.type = FT_REACTOR;
     info.id   = reactor->id;

     direct_snputs( info.name, name, sizeof(info.name) );

     ioctl( world->fusion_fd, FUSION_ENTRY_SET_INFO, &info );

     return reactor;
}

DirectResult
fusion_reactor_destroy( FusionReactor *reactor )
{
     FusionWorldShared *shared;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     shared = reactor->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "fusion_reactor_destroy( %p [%d] )\n", reactor, reactor->id );

     D_ASSUME( !reactor->destroyed );

     if (reactor->destroyed)
          return DFB_DESTROYED;

     while (ioctl( _fusion_fd( shared ), FUSION_REACTOR_DESTROY, &reactor->id )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Reactor: invalid reactor\n" );
                    return DFB_DESTROYED;
          }

          D_PERROR( "FUSION_REACTOR_DESTROY" );
          return DFB_FUSION;
     }

     reactor->destroyed = true;

     return DFB_OK;
}

DirectResult
fusion_reactor_free( FusionReactor *reactor )
{
     FusionWorldShared *shared;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     shared = reactor->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "fusion_reactor_free( %p [%d] )\n", reactor, reactor->id );

     D_MAGIC_CLEAR( reactor );

//     D_ASSUME( reactor->destroyed );

     if (!reactor->destroyed)
          while (ioctl( _fusion_fd( shared ), FUSION_REACTOR_DESTROY, &reactor->id ) && errno == EINTR);

     /* free shared reactor data */
     SHFREE( shared->main_pool, reactor );

     return DFB_OK;
}

DirectResult
fusion_reactor_attach_channel( FusionReactor *reactor,
                               int            channel,
                               ReactionFunc   func,
                               void          *ctx,
                               Reaction      *reaction )
{
     ReactorNode         *node;
     NodeLink            *link;
     FusionReactorAttach  attach;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( func != NULL );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_attach( %p [%d], func %p, ctx %p, reaction %p )\n",
                 reactor, reactor->id, func, ctx, reaction );

     link = D_CALLOC( 1, sizeof(NodeLink) );
     if (!link)
          return D_OOM();

     node = lock_node( reactor->id, true, true, reactor, NULL );
     if (!node) {
          D_FREE( link );
          return DFB_FUSION;
     }

     attach.reactor_id = reactor->id;
     attach.channel    = channel;

     while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_ATTACH, &attach )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Reactor: invalid reactor\n" );
                    unlock_node( node );
                    D_FREE( link );
                    return DFB_DESTROYED;
          }

          D_PERROR( "FUSION_REACTOR_ATTACH" );
          unlock_node( node );
          D_FREE( link );
          return DFB_FUSION;
     }

     /* fill out callback information */
     reaction->func      = func;
     reaction->ctx       = ctx;
     reaction->node_link = link;

     link->reaction = reaction;
     link->channel  = channel;

     D_MAGIC_SET( link, NodeLink );

     /* prepend the reaction to the local reaction list */
     direct_list_prepend( &node->links, &link->link );

     unlock_node( node );

     return DFB_OK;
}

static void
remove_node_link( ReactorNode *node,
                  NodeLink    *link )
{
     D_MAGIC_ASSERT( node, ReactorNode );
     D_MAGIC_ASSERT( link, NodeLink );

     D_ASSUME( link->reaction == NULL );

     direct_list_remove( &node->links, &link->link );

     D_MAGIC_CLEAR( link );

     D_FREE( link );
}

DirectResult
fusion_reactor_detach( FusionReactor *reactor,
                       Reaction      *reaction )
{
     ReactorNode *node;
     NodeLink    *link;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_detach( %p [%d], reaction %p ) <- func %p, ctx %p\n",
                 reactor, reactor->id, reaction, reaction->func, reaction->ctx );

     node = lock_node( reactor->id, false, true, reactor, NULL );
     if (!node) {
          D_BUG( "node not found" );
          return DFB_BUG;
     }

     link = reaction->node_link;
     D_ASSUME( link != NULL );

     if (link) {
          FusionReactorDetach detach;

          D_ASSERT( link->reaction == reaction );

          detach.reactor_id = reactor->id;
          detach.channel    = link->channel;

          reaction->node_link = NULL;

          link->reaction = NULL;

          remove_node_link( node, link );

          while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_DETACH, &detach )) {
               switch (errno) {
                    case EINTR:
                         continue;

                    case EINVAL:
                         D_ERROR( "Fusion/Reactor: invalid reactor\n" );
                         unlock_node( node );
                         return DFB_DESTROYED;
               }

               D_PERROR( "FUSION_REACTOR_DETACH" );
               unlock_node( node );
               return DFB_FUSION;
          }
     }

     unlock_node( node );

     return DFB_OK;
}

DirectResult
fusion_reactor_dispatch_channel( FusionReactor      *reactor,
                                 int                 channel,
                                 const void         *msg_data,
                                 int                 msg_size,
                                 bool                self,
                                 const ReactionFunc *globals )
{
     FusionReactorDispatch dispatch;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_ASSERT( msg_data != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_dispatch( %p [%d], msg_data %p, self %s, globals %p)\n",
                 reactor, reactor->id, msg_data, self ? "true" : "false", globals );

     /* Handle global reactions first. */
     if (reactor->globals) {
          if (globals)
               process_globals( reactor, msg_data, globals );
          else
               D_ERROR( "Fusion/Reactor: global reactions exist but no "
                        "globals have been passed to dispatch()\n" );
     }
     
     /* Handle local reactions. */
     if (self && reactor->direct) {
          _fusion_reactor_process_message( _fusion_world(reactor->shared), reactor->id, channel, msg_data );
          self = false;
     }

     /* Initialize dispatch data. */
     dispatch.reactor_id = reactor->id;
     dispatch.channel    = channel;
     dispatch.self       = self;
     dispatch.msg_size   = msg_size;
     dispatch.msg_data   = msg_data;

     /* Dispatch the message to handle foreign reactions. */
     while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_DISPATCH, &dispatch )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Reactor: invalid reactor\n" );
                    return DFB_DESTROYED;
          }

          D_PERROR( "FUSION_REACTOR_DISPATCH" );
          return DFB_FUSION;
     }

     return DFB_OK;
}

DirectResult
fusion_reactor_set_dispatch_callback( FusionReactor  *reactor,
                                      FusionCall     *call,
                                      void           *call_ptr )
{
     FusionReactorSetCallback callback;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( call != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_set_dispatch_callback( %p [%d], call %p [%d], ptr %p)\n",
                 reactor, reactor->id, call, call->call_id, call_ptr );

     /* Fill callback info. */
     callback.reactor_id = reactor->id;
     callback.call_id    = call->call_id;
     callback.call_ptr   = call_ptr;

     /* Set the dispatch callback. */
     while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_SET_DISPATCH_CALLBACK, &callback )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Reactor: invalid reactor\n" );
                    return DFB_DESTROYED;
          }

          D_PERROR( "FUSION_REACTOR_SET_DISPATCH_CALLBACK" );
          return DFB_FUSION;
     }

     return DFB_OK;
}


void
_fusion_reactor_process_message( FusionWorld *world,
                                 int          reactor_id,
                                 int          channel,
                                 const void  *msg_data )
{
     ReactorNode *node;
     NodeLink    *link;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_ASSERT( msg_data != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "  _fusion_reactor_process_message( [%d], msg_data %p )\n", reactor_id, msg_data );

     /* Find the local counter part of the reactor. */
     node = lock_node( reactor_id, false, false, NULL, world );
     if (!node)
          return;

     D_DEBUG_AT( Fusion_Reactor, "    -> node %p, reactor %p\n", node, node->reactor );

     D_ASSUME( node->links != NULL );

     if (!node->links) {
          D_DEBUG_AT( Fusion_Reactor, "    -> no local reactions!?!\n" );
          unlock_node( node );
          return;
     }

     direct_list_foreach (link, node->links) {
          Reaction *reaction;

          D_MAGIC_ASSERT( link, NodeLink );

          if (link->channel != channel)
               continue;

          reaction = link->reaction;
          if (!reaction)
               continue;

          if (reaction->func( msg_data, reaction->ctx ) == RS_REMOVE) {
               FusionReactorDetach detach;

               detach.reactor_id = reactor_id;
               detach.channel    = channel;

               D_DEBUG_AT( Fusion_Reactor, "    -> removing %p, func %p, ctx %p\n",
                           reaction, reaction->func, reaction->ctx );

               link->reaction = NULL;

               /* We can't remove the link as we only have read lock, to avoid dead locks. */

               while (ioctl( world->fusion_fd, FUSION_REACTOR_DETACH, &detach )) {
                    switch (errno) {
                         case EINTR:
                              continue;

                         case EINVAL:
                              D_ERROR( "Fusion/Reactor: invalid reactor (DETACH)\n" );
                              break;

                         default:
                              D_PERROR( "FUSION_REACTOR_DETACH" );
                              break;
                    }

                    break;
               }
          }
     }

     unlock_node( node );
}

#else /* FUSION_BUILD_KERNEL */

typedef struct {
     DirectLink    link;
     
     unsigned int  refs;
     
     FusionID      fusion_id;
     int           channel;
} __Listener;


FusionReactor *
fusion_reactor_new( int                msg_size,
                    const char        *name,
                    const FusionWorld *world )
{
     FusionReactor     *reactor;
     FusionWorldShared *shared;

     D_ASSERT( msg_size > 0 );
     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );

     shared = world->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "fusion_reactor_new( '%s', size %d )\n", name ? : "", msg_size );

     /* allocate shared reactor data */
     reactor = SHCALLOC( shared->main_pool, 1, sizeof (FusionReactor) );
     if (!reactor) {
          D_OOSHM();
          return NULL;
     }
     
     /* Generate the reactor id */
     reactor->id = ++shared->reactor_ids; 

     /* Set the static message size, should we make dynamic? (TODO?) */
     reactor->msg_size = msg_size;
     
     /* Set default lock for global reactions. */
     reactor->globals_lock = &shared->reactor_globals;
     
     fusion_skirmish_init( &reactor->listeners_lock, "Reactor Listeners", world );

     D_DEBUG_AT( Fusion_Reactor, "  -> new reactor %p [%d] with lock %p [%d]\n",
                 reactor, reactor->id, reactor->globals_lock, reactor->globals_lock->multi.id );

     reactor->shared = shared;
     reactor->direct = true;

     D_MAGIC_SET( reactor, FusionReactor );

     return reactor;
}

DirectResult
fusion_reactor_destroy( FusionReactor *reactor )
{
     FusionWorldShared *shared;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     shared = reactor->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "fusion_reactor_destroy( %p [%d] )\n", reactor, reactor->id );

     D_ASSUME( !reactor->destroyed );

     if (reactor->destroyed)
          return DFB_DESTROYED;
          
     fusion_skirmish_destroy( &reactor->listeners_lock );
          
     reactor->destroyed = true;

     return DFB_OK;
}

DirectResult
fusion_reactor_free( FusionReactor *reactor )
{
     FusionWorldShared *shared;
     __Listener        *listener, *temp;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     shared = reactor->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "fusion_reactor_free( %p [%d] )\n", reactor, reactor->id );

     D_MAGIC_CLEAR( reactor );

//     D_ASSUME( reactor->destroyed );

     direct_list_foreach_safe (listener, temp, reactor->listeners) {
          direct_list_remove( &reactor->listeners, &listener->link );
          SHFREE( shared->main_pool, listener );
     }

     /* free shared reactor data */
     SHFREE( shared->main_pool, reactor );

     return DFB_OK;
}

DirectResult
fusion_reactor_attach_channel( FusionReactor *reactor,
                               int            channel,
                               ReactionFunc   func,
                               void          *ctx,
                               Reaction      *reaction )
{
     FusionWorldShared *shared;
     ReactorNode       *node;
     NodeLink          *link;
     FusionID           fusion_id;
     __Listener        *listener;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( func != NULL );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_attach( %p [%d], func %p, ctx %p, reaction %p )\n",
                 reactor, reactor->id, func, ctx, reaction );
                 
     if (reactor->destroyed)
          return DFB_DESTROYED;
                 
     shared = reactor->shared;

     link = D_CALLOC( 1, sizeof(NodeLink) );
     if (!link)
          return D_OOM();

     node = lock_node( reactor->id, true, true, reactor, NULL );
     if (!node) {
          D_FREE( link );
          return DFB_FUSION;
     }
     
     fusion_id = _fusion_id( shared );
     
     fusion_skirmish_prevail( &reactor->listeners_lock );
     
     direct_list_foreach (listener, reactor->listeners) {
          if (listener->fusion_id == fusion_id && listener->channel == channel) {
               listener->refs++;
               break;
          }
     }
     
     if (!listener) {
          listener = SHCALLOC( shared->main_pool, 1, sizeof(__Listener) );
          if (!listener) {
               D_OOSHM();
               fusion_skirmish_dismiss( &reactor->listeners_lock );
               unlock_node( node );
               D_FREE( link );
               return DFB_NOSHAREDMEMORY;
          }
          
          listener->refs      = 1;
          listener->fusion_id = fusion_id;
          listener->channel   = channel;
         
          direct_list_append( &reactor->listeners, &listener->link );
     }
     
     fusion_skirmish_dismiss( &reactor->listeners_lock );

     /* fill out callback information */
     reaction->func      = func;
     reaction->ctx       = ctx;
     reaction->node_link = link;

     link->reaction = reaction;
     link->channel  = channel;

     D_MAGIC_SET( link, NodeLink );

     /* prepend the reaction to the local reaction list */
     direct_list_prepend( &node->links, &link->link );

     unlock_node( node );

     return DFB_OK;
}

static void
remove_node_link( ReactorNode *node,
                  NodeLink    *link )
{
     D_MAGIC_ASSERT( node, ReactorNode );
     D_MAGIC_ASSERT( link, NodeLink );

     D_ASSUME( link->reaction == NULL );

     direct_list_remove( &node->links, &link->link );

     D_MAGIC_CLEAR( link );

     D_FREE( link );
}

DirectResult
fusion_reactor_detach( FusionReactor *reactor,
                       Reaction      *reaction )
{
     FusionWorldShared *shared;
     ReactorNode       *node;
     NodeLink          *link;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_detach( %p [%d], reaction %p ) <- func %p, ctx %p\n",
                 reactor, reactor->id, reaction, reaction->func, reaction->ctx );
     
     if (reactor->destroyed)
          return DFB_DESTROYED;
                          
     shared = reactor->shared;

     node = lock_node( reactor->id, false, true, reactor, NULL );
     if (!node) {
          D_BUG( "node not found" );
          return DFB_BUG;
     }

     link = reaction->node_link;
     D_ASSUME( link != NULL );

     if (link) {
          __Listener *listener;
          FusionID    fusion_id = _fusion_id( shared );

          D_ASSERT( link->reaction == reaction );

          reaction->node_link = NULL;

          link->reaction = NULL;

          remove_node_link( node, link );
          
          fusion_skirmish_prevail( &reactor->listeners_lock );
          
          direct_list_foreach (listener, reactor->listeners) {
               if (listener->fusion_id == fusion_id && listener->channel == link->channel) {
                    if (--listener->refs == 0) {
                         direct_list_remove( &reactor->listeners, &listener->link );
                         SHFREE( shared->main_pool, listener );
                    }
                    break;
               }
          }
           
          fusion_skirmish_dismiss( &reactor->listeners_lock );
          
          if (!listener)
               D_ERROR( "Fusion/Reactor: Couldn't detach listener!\n" );
     }

     unlock_node( node );

     return DFB_OK;
}

DirectResult
fusion_reactor_dispatch_channel( FusionReactor      *reactor,
                                 int                 channel,
                                 const void         *msg_data,
                                 int                 msg_size,
                                 bool                self,
                                 const ReactionFunc *globals )
{
     FusionWorld           *world;
     __Listener            *listener, *temp; 
     FusionReactorMessage  *msg;
     struct sockaddr_un     addr;
     int                    len;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_ASSERT( msg_data != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_dispatch( %p [%d], msg_data %p, self %s, globals %p)\n",
                 reactor, reactor->id, msg_data, self ? "true" : "false", globals );

     if (reactor->destroyed)
          return DFB_DESTROYED;

     if (msg_size > FUSION_MESSAGE_SIZE-sizeof(FusionReactorMessage)) {
          D_ERROR( "Fusion/Reactor: Message too large (%d)!\n", msg_size );
          return DFB_UNSUPPORTED;
     }

     /* Handle global reactions first. */
     if (reactor->globals) {
          if (globals)
               process_globals( reactor, msg_data, globals );
          else
               D_ERROR( "Fusion/Reactor: global reactions exist but no "
                        "globals have been passed to dispatch()\n" );
     }
     
     /* Handle local reactions. */
     if (self && reactor->direct) {
          _fusion_reactor_process_message( _fusion_world(reactor->shared), reactor->id, channel, msg_data );
          self = false;
     }
     
     world = _fusion_world( reactor->shared );

     msg = alloca( sizeof(FusionReactorMessage) + msg_size );
     
     msg->type    = FMT_REACTOR;
     msg->id      = reactor->id;
     msg->channel = channel;
     
     memcpy( (void*)msg + sizeof(FusionReactorMessage), msg_data, msg_size );

     addr.sun_family = AF_UNIX;
     len = snprintf( addr.sun_path, sizeof(addr.sun_path), 
                     "/tmp/fusion.%d/", fusion_world_index( world ) );
     
     fusion_skirmish_prevail( &reactor->listeners_lock );
     
     direct_list_foreach_safe (listener, temp, reactor->listeners) {
          if (listener->channel == channel) {
               DirectResult ret;
               
               if (!self && listener->fusion_id == world->fusion_id)
                    continue;

               snprintf( addr.sun_path+len, sizeof(addr.sun_path)-len, "%lx", listener->fusion_id );

               D_DEBUG_AT( Fusion_Reactor, " -> sending to '%s'\n", addr.sun_path );
               
               ret = _fusion_send_message( world->fusion_fd, msg, sizeof(FusionReactorMessage)+msg_size, &addr );
               if (ret == DFB_DEAD) {
                    D_DEBUG_AT( Fusion_Reactor, " -> removing dead listener %lu\n", listener->fusion_id );
                    direct_list_remove( &reactor->listeners, &listener->link ); 
                    SHFREE( reactor->shared->main_pool, listener );
               }
          }
     }
     
     fusion_skirmish_dismiss( &reactor->listeners_lock );

     D_DEBUG_AT( Fusion_Reactor, "fusion_reactor_dispatch( %p ) done.\n", reactor );

     return DFB_OK;
}

DirectResult
fusion_reactor_set_dispatch_callback( FusionReactor  *reactor,
                                      FusionCall     *call,
                                      void           *call_ptr )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( call != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_set_dispatch_callback( %p [%d], call %p [%d], ptr %p)\n",
                 reactor, reactor->id, call, call->call_id, call_ptr );
                 
     if (reactor->destroyed)
          return DFB_DESTROYED;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

void
_fusion_reactor_process_message( FusionWorld *world,
                                 int          reactor_id,
                                 int          channel,
                                 const void  *msg_data )
{
     ReactorNode *node;
     NodeLink    *link;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_ASSERT( msg_data != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "  _fusion_reactor_process_message( [%d], msg_data %p )\n", reactor_id, msg_data );

     /* Find the local counter part of the reactor. */
     node = lock_node( reactor_id, false, false, NULL, world );
     if (!node)
          return;

     D_DEBUG_AT( Fusion_Reactor, "    -> node %p, reactor %p\n", node, node->reactor );

     D_ASSUME( node->links != NULL );

     if (!node->links) {
          D_DEBUG_AT( Fusion_Reactor, "    -> no local reactions!?!\n" );
          unlock_node( node );
          return;
     }

     direct_list_foreach (link, node->links) {
          Reaction *reaction;

          D_MAGIC_ASSERT( link, NodeLink );

          if (link->channel != channel)
               continue;

          reaction = link->reaction;
          if (!reaction)
               continue;

          if (reaction->func( msg_data, reaction->ctx ) == RS_REMOVE) {
               FusionReactor *reactor = node->reactor;
               __Listener    *listener;
               
               D_DEBUG_AT( Fusion_Reactor, "    -> removing %p, func %p, ctx %p\n",
                           reaction, reaction->func, reaction->ctx );
               
               fusion_skirmish_prevail( &reactor->listeners_lock );
               
               direct_list_foreach (listener, reactor->listeners) {
                    if (listener->fusion_id == world->fusion_id && listener->channel == channel) {
                         if (--listener->refs == 0) {
                              direct_list_remove( &reactor->listeners, &listener->link );
                              SHFREE( world->shared->main_pool, listener );
                         }
                         break;
                    }
               }
               
               fusion_skirmish_dismiss( &reactor->listeners_lock );
          }
     }

     unlock_node( node );
}

#endif /* FUSION_BUILD_KERNEL */


DirectResult
fusion_reactor_set_lock( FusionReactor  *reactor,
                         FusionSkirmish *lock )
{
     DirectResult    ret;
     FusionSkirmish *old;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     old = reactor->globals_lock;

     D_ASSERT( lock != NULL );
     D_ASSERT( old != NULL );

     D_DEBUG_AT( Fusion_Reactor, "fusion_reactor_set_lock( %p [%d], lock %p [%d] ) <- old %p [%d]\n",
                 reactor, reactor->id, lock, lock->multi.id, old, old->multi.id );

     /*
      * Acquire the old lock to make sure that changing the lock doesn't
      * result in mismatching lock/unlock pairs in other functions.
      */
     ret = fusion_skirmish_prevail( old );
     if (ret)
          return ret;

     D_ASSUME( reactor->globals_lock != lock );

     /* Set the lock replacement. */
     reactor->globals_lock = lock;

     /* Release the old lock which is obsolete now. */
     fusion_skirmish_dismiss( old );

     return DFB_OK;
}

DirectResult
fusion_reactor_set_lock_only( FusionReactor  *reactor,
                              FusionSkirmish *lock )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( lock != NULL );

     D_DEBUG_AT( Fusion_Reactor, "fusion_reactor_set_lock_only( %p [%d], lock %p [%d] ) <- old %p [%d]\n",
                 reactor, reactor->id, lock, lock->multi.id, reactor->globals_lock, reactor->globals_lock->multi.id );

     D_ASSUME( reactor->globals_lock != lock );

     /* Set the lock replacement. */
     reactor->globals_lock = lock;

     return DFB_OK;
}

DirectResult
fusion_reactor_attach (FusionReactor *reactor,
                       ReactionFunc   func,
                       void          *ctx,
                       Reaction      *reaction)
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( func != NULL );
     D_ASSERT( reaction != NULL );

     return fusion_reactor_attach_channel( reactor, 0, func, ctx, reaction );
}

DirectResult
fusion_reactor_attach_global( FusionReactor  *reactor,
                              int             index,
                              void           *ctx,
                              GlobalReaction *reaction )
{
     DirectResult    ret;
     FusionSkirmish *lock;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_ASSERT( index >= 0 );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_attach_global( %p [%d], index %d, ctx %p, reaction %p )\n",
                 reactor, reactor->id, index, ctx, reaction );

     /* Initialize reaction data. */
     reaction->index    = index;
     reaction->ctx      = ctx;
     reaction->attached = true;

     /* Remember for safety. */
     lock = reactor->globals_lock;

     D_ASSERT( lock != NULL );

     /* Lock the list of global reactions. */
     ret = fusion_skirmish_prevail( lock );
     if (ret)
          return ret;

     /* FIXME: Might have changed while waiting for the lock. */
     if (lock != reactor->globals_lock)
          D_WARN( "using old lock once more" );

     /* Prepend the reaction to the list. */
     direct_list_prepend( &reactor->globals, &reaction->link );

     /* Unlock the list of global reactions. */
     fusion_skirmish_dismiss( lock );

     return DFB_OK;
}

DirectResult
fusion_reactor_detach_global( FusionReactor  *reactor,
                              GlobalReaction *reaction )
{
     DirectResult    ret;
     FusionSkirmish *lock;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_detach_global( %p [%d], reaction %p ) <- index %d, ctx %p\n",
                 reactor, reactor->id, reaction, reaction->index, reaction->ctx );

     /* Remember for safety. */
     lock = reactor->globals_lock;

     D_ASSERT( lock != NULL );

     /* Lock the list of global reactions. */
     ret = fusion_skirmish_prevail( lock );
     if (ret)
          return ret;

     /* FIXME: Might have changed while waiting for the lock. */
     if (lock != reactor->globals_lock)
          D_WARN( "using old lock once more" );

     D_ASSUME( reaction->attached );

     /* Check against multiple detach. */
     if (reaction->attached) {
          /* Mark as detached. */
          reaction->attached = false;

          /* Remove the reaction from the list. */
          direct_list_remove( &reactor->globals, &reaction->link );
     }

     /* Unlock the list of global reactions. */
     fusion_skirmish_dismiss( lock );

     return DFB_OK;
}

DirectResult
fusion_reactor_dispatch( FusionReactor      *reactor,
                         const void         *msg_data,
                         bool                self,
                         const ReactionFunc *globals )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     return fusion_reactor_dispatch_channel( reactor, 0, msg_data, reactor->msg_size, self, globals );
}

DirectResult
fusion_reactor_sized_dispatch( FusionReactor      *reactor,
                               const void         *msg_data,
                               int                 msg_size,
                               bool                self,
                               const ReactionFunc *globals )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     return fusion_reactor_dispatch_channel( reactor, 0, msg_data, msg_size, self, globals );
}

DirectResult
fusion_reactor_direct( FusionReactor *reactor, bool direct )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     
     reactor->direct = direct;
     
     return DFB_OK;
}


void
_fusion_reactor_free_all( FusionWorld *world )
{
     ReactorNode *node, *node_temp;

     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Reactor, "_fusion_reactor_free_all() <- nodes %p\n", world->reactor_nodes );


     pthread_mutex_lock( &world->reactor_nodes_lock );

     direct_list_foreach_safe (node, node_temp, world->reactor_nodes) {
          NodeLink *link, *link_temp;

          D_MAGIC_ASSERT( node, ReactorNode );

          pthread_rwlock_wrlock( &node->lock );

          direct_list_foreach_safe (link, link_temp, node->links) {
               D_MAGIC_ASSERT( link, NodeLink );

               D_MAGIC_CLEAR( link );

               D_FREE( link );
          }

          pthread_rwlock_unlock( &node->lock );
          pthread_rwlock_destroy( &node->lock );

          D_MAGIC_CLEAR( node );

          D_FREE( node );
     }

     world->reactor_nodes = NULL;

     pthread_mutex_unlock( &world->reactor_nodes_lock );
}

static void
process_globals( FusionReactor      *reactor,
                 const void         *msg_data,
                 const ReactionFunc *globals )
{
     DirectLink     *n;
     GlobalReaction *global;
     FusionSkirmish *lock;
     int             max_index = -1;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_ASSERT( msg_data != NULL );
     D_ASSERT( globals != NULL );

/*     D_DEBUG_AT( Fusion_Reactor, "  process_globals( %p [%d], msg_data %p, globals %p )\n",
                 reactor, reactor->id, msg_data, globals );*/

     /* Find maximum reaction index. */
     while (globals[max_index+1])
          max_index++;

     if (max_index < 0)
          return;

     /* Remember for safety. */
     lock = reactor->globals_lock;

     D_ASSERT( lock != NULL );

     /* Lock the list of global reactions. */
     if (fusion_skirmish_prevail( lock ))
          return;

     /* FIXME: Might have changed while waiting for the lock. */
     if (lock != reactor->globals_lock)
          D_WARN( "using old lock once more" );

     /* Loop through all global reactions. */
     direct_list_foreach_safe (global, n, reactor->globals) {
          int index = global->index;

          /* Check if the index is valid. */
          if (index < 0 || index > max_index) {
               D_WARN( "index out of bounds (%d/%d)", global->index, max_index );
               continue;
          }

          /* Call reaction and remove it if requested. */
          if (globals[global->index]( msg_data, global->ctx ) == RS_REMOVE) {
               /*D_DEBUG_AT( Fusion_Reactor, "    -> removing %p, index %d, ctx %p\n",
                           global, global->index, global->ctx );*/

               direct_list_remove( &reactor->globals, &global->link );
          }
     }

     /* Unlock the list of global reactions. */
     fusion_skirmish_dismiss( lock );
}



/*****************************
 *  File internal functions  *
 *****************************/

static ReactorNode *
lock_node( int reactor_id, bool add_it, bool wlock, FusionReactor *reactor, FusionWorld *world )
{
     DirectLink        *n;
     ReactorNode       *node;
     FusionWorldShared *shared;

     D_DEBUG_AT( Fusion_Reactor, "    lock_node( [%d], add %s, reactor %p )\n",
                 reactor_id, add_it ? "true" : "false", reactor );

     D_ASSERT( reactor != NULL || (!add_it && world != NULL) );

     if (reactor) {
          D_MAGIC_ASSERT( reactor, FusionReactor );

          shared = reactor->shared;

          D_MAGIC_ASSERT( shared, FusionWorldShared );

          world = _fusion_world( shared );

          D_MAGIC_ASSERT( world, FusionWorld );
     }
     else {
          D_MAGIC_ASSERT( world, FusionWorld );

          shared = world->shared;

          D_MAGIC_ASSERT( shared, FusionWorldShared );
     }


     pthread_mutex_lock( &world->reactor_nodes_lock );

     direct_list_foreach_safe (node, n, world->reactor_nodes) {
          D_MAGIC_ASSERT( node, ReactorNode );

          if (node->reactor_id == reactor_id) {
               if (wlock) {
                    DirectLink *n;
                    NodeLink   *link;

                    pthread_rwlock_wrlock( &node->lock );

                    /* FIXME: don't cleanup asynchronously */
                    direct_list_foreach_safe (link, n, node->links) {
                         D_MAGIC_ASSERT( link, NodeLink );

                         if (!link->reaction) {
                              D_DEBUG_AT( Fusion_Reactor, "    -> cleaning up %p\n", link );
                         
                              remove_node_link( node, link );
                         }
                         else
                              D_ASSERT( link->reaction->node_link == link );
                    }
               }
               else
                    pthread_rwlock_rdlock( &node->lock );

               /* FIXME: Don't cleanup asynchronously. */
               if (!node->links && !add_it) {
//                    D_DEBUG_AT( Fusion_Reactor, "      -> cleaning up mine %p\n", node );

                    direct_list_remove( &world->reactor_nodes, &node->link );

                    pthread_rwlock_unlock( &node->lock );
                    pthread_rwlock_destroy( &node->lock );

                    D_MAGIC_CLEAR( node );

                    D_FREE( node );

                    node = NULL;
               }
               else {
/*                    D_DEBUG_AT( Fusion_Reactor, "      -> found %p (%d reactions)\n",
                                node, direct_list_count_elements_EXPENSIVE( node->reactions ) );*/

                    D_ASSERT( node->reactor == reactor || reactor == NULL );

                    direct_list_move_to_front( &world->reactor_nodes, &node->link );
               }

               pthread_mutex_unlock( &world->reactor_nodes_lock );

               return node;
          }

          /* FIXME: Don't cleanup asynchronously. */
          if (!pthread_rwlock_trywrlock( &node->lock )) {
               if (!node->links) {
//                    D_DEBUG_AT( Fusion_Reactor, "      -> cleaning up other %p\n", node );

                    direct_list_remove( &world->reactor_nodes, &node->link );

                    pthread_rwlock_unlock( &node->lock );
                    pthread_rwlock_destroy( &node->lock );

                    D_MAGIC_CLEAR( node );

                    D_FREE( node );
               }
               else {
                    /*D_DEBUG_AT( Fusion_Reactor, "      -> keeping other %p (%d reactions)\n",
                                node, direct_list_count_elements_EXPENSIVE( node->reactions ) );*/

                    pthread_rwlock_unlock( &node->lock );
               }
          }
     }

//     D_DEBUG_AT( Fusion_Reactor, "      -> not found%s adding\n", add_it ? ", but" : " and not" );

     if (add_it) {
          D_MAGIC_ASSERT( reactor, FusionReactor );

          node = D_CALLOC( 1, sizeof(ReactorNode) );
          if (!node) {
               D_OOM();
               return NULL;
          }

          //direct_util_recursive_pthread_mutex_init( &node->lock );
          pthread_rwlock_init( &node->lock, NULL );


          if (wlock)
               pthread_rwlock_wrlock( &node->lock );
          else
               pthread_rwlock_rdlock( &node->lock );

          node->reactor_id = reactor_id;
          node->reactor    = reactor;

          D_MAGIC_SET( node, ReactorNode );

          direct_list_prepend( &world->reactor_nodes, &node->link );

          pthread_mutex_unlock( &world->reactor_nodes_lock );

          return node;
     }

     pthread_mutex_unlock( &world->reactor_nodes_lock );

     return NULL;
}

static void
unlock_node( ReactorNode *node )
{
     D_ASSERT( node != NULL );

//     D_MAGIC_ASSERT( node->reactor, FusionReactor );

/*     D_DEBUG_AT( Fusion_Reactor, "    unlock_node( %p, reactor %p [%d] )\n",
                 node, node->reactor, node->reactor->id );*/

     pthread_rwlock_unlock( &node->lock );
}

#else /* FUSION_BUILD_MULTI */

/***************************
 *  Internal declarations  *
 ***************************/

/*
 *
 */
struct __Fusion_FusionReactor {
     DirectLink       *reactions; /* reactor listeners attached to node  */
     pthread_mutex_t   reactions_lock;

     DirectLink       *globals; /* global reactions attached to node  */
     pthread_mutex_t   globals_lock;

     bool              destroyed;
};

static void
process_globals( FusionReactor      *reactor,
                 const void         *msg_data,
                 const ReactionFunc *globals );

/****************
 *  Public API  *
 ****************/

FusionReactor *
fusion_reactor_new( int                msg_size,
                    const char        *name,
                    const FusionWorld *world )
{
     FusionReactor *reactor;

     D_ASSERT( msg_size > 0 );

     reactor = D_CALLOC( 1, sizeof(FusionReactor) );
     if (!reactor)
          return NULL;

     direct_util_recursive_pthread_mutex_init( &reactor->reactions_lock );
     direct_util_recursive_pthread_mutex_init( &reactor->globals_lock );

     return reactor;
}

DirectResult
fusion_reactor_set_lock( FusionReactor  *reactor,
                         FusionSkirmish *lock )
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( lock != NULL );

//     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

DirectResult
fusion_reactor_set_lock_only( FusionReactor  *reactor,
                              FusionSkirmish *lock )
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( lock != NULL );

     return DFB_UNIMPLEMENTED;
}

DirectResult
fusion_reactor_attach (FusionReactor *reactor,
                       ReactionFunc   func,
                       void          *ctx,
                       Reaction      *reaction)
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( func != NULL );
     D_ASSERT( reaction != NULL );

     reaction->func = func;
     reaction->ctx  = ctx;

     pthread_mutex_lock( &reactor->reactions_lock );

     direct_list_prepend( &reactor->reactions, &reaction->link );

     pthread_mutex_unlock( &reactor->reactions_lock );

     return DFB_OK;
}

DirectResult
fusion_reactor_detach (FusionReactor *reactor,
                       Reaction      *reaction)
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( reaction != NULL );

     pthread_mutex_lock( &reactor->reactions_lock );

     direct_list_remove( &reactor->reactions, &reaction->link );

     pthread_mutex_unlock( &reactor->reactions_lock );

     return DFB_OK;
}

DirectResult
fusion_reactor_attach_global (FusionReactor  *reactor,
                              int             index,
                              void           *ctx,
                              GlobalReaction *reaction)
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( index >= 0 );
     D_ASSERT( reaction != NULL );

     reaction->index = index;
     reaction->ctx   = ctx;

     pthread_mutex_lock( &reactor->globals_lock );

     direct_list_prepend( &reactor->globals, &reaction->link );

     pthread_mutex_unlock( &reactor->globals_lock );

     return DFB_OK;
}

DirectResult
fusion_reactor_detach_global (FusionReactor  *reactor,
                              GlobalReaction *reaction)
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( reaction != NULL );

     pthread_mutex_lock( &reactor->globals_lock );

     direct_list_remove( &reactor->globals, &reaction->link );

     pthread_mutex_unlock( &reactor->globals_lock );

     return DFB_OK;
}

DirectResult
fusion_reactor_attach_channel( FusionReactor *reactor,
                               int            channel,
                               ReactionFunc   func,
                               void          *ctx,
                               Reaction      *reaction )
{
     return DFB_UNIMPLEMENTED;
}

DirectResult
fusion_reactor_dispatch_channel( FusionReactor      *reactor,
                                 int                 channel,
                                 const void         *msg_data,
                                 int                 msg_size,
                                 bool                self,
                                 const ReactionFunc *globals )
{
     return DFB_UNIMPLEMENTED;
}

DirectResult
fusion_reactor_set_dispatch_callback( FusionReactor  *reactor,
                                      FusionCall     *call,
                                      void           *call_ptr )
{
     return DFB_UNIMPLEMENTED;
}

DirectResult
fusion_reactor_dispatch (FusionReactor      *reactor,
                         const void         *msg_data,
                         bool                self,
                         const ReactionFunc *globals)
{
     DirectLink *l;

     D_ASSERT( reactor != NULL );
     D_ASSERT( msg_data != NULL );

     if (reactor->globals) {
          if (globals)
               process_globals( reactor, msg_data, globals );
          else
               D_ERROR( "Fusion/Reactor: global reactions exist but no "
                        "globals have been passed to dispatch()\n" );
     }

     if (!self)
          return DFB_OK;

     pthread_mutex_lock( &reactor->reactions_lock );

     l = reactor->reactions;
     while (l) {
          DirectLink *next     = l->next;
          Reaction   *reaction = (Reaction*) l;

          switch (reaction->func( msg_data, reaction->ctx )) {
               case RS_REMOVE:
                    direct_list_remove( &reactor->reactions, l );
                    break;

               case RS_DROP:
                    pthread_mutex_unlock( &reactor->reactions_lock );
                    return DFB_OK;

               default:
                    break;
          }

          l = next;
     }

     pthread_mutex_unlock( &reactor->reactions_lock );

     return DFB_OK;
}

DirectResult
fusion_reactor_direct( FusionReactor *reactor, bool direct )
{
     D_ASSERT( reactor != NULL );
     
     return DFB_OK;
}

DirectResult
fusion_reactor_destroy (FusionReactor *reactor)
{
     D_ASSERT( reactor != NULL );

     D_ASSUME( !reactor->destroyed );

     reactor->destroyed = true;

     return DFB_OK;
}

DirectResult
fusion_reactor_free (FusionReactor *reactor)
{
     D_ASSERT( reactor != NULL );

//     D_ASSUME( reactor->destroyed );

     reactor->reactions = NULL;

     pthread_mutex_destroy( &reactor->reactions_lock );

     D_FREE( reactor );

     return DFB_OK;
}

/******************************************************************************/

static void
process_globals( FusionReactor      *reactor,
                 const void         *msg_data,
                 const ReactionFunc *globals )
{
     DirectLink     *n;
     GlobalReaction *global;
     int             max_index = -1;

     D_ASSERT( reactor != NULL );
     D_ASSERT( msg_data != NULL );
     D_ASSERT( globals != NULL );

     while (globals[max_index+1])
          max_index++;

     if (max_index < 0)
          return;

     pthread_mutex_lock( &reactor->globals_lock );

     direct_list_foreach_safe (global, n, reactor->globals) {
          if (global->index < 0 || global->index > max_index) {
               D_WARN( "global reaction index out of bounds (%d/%d)", global->index, max_index );
          }
          else {
               if (globals[ global->index ]( msg_data, global->ctx ) == RS_REMOVE)
                    direct_list_remove( &reactor->globals, &global->link );
          }
     }

     pthread_mutex_unlock( &reactor->globals_lock );
}

#endif

