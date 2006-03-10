/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#if FUSION_BUILD_MULTI
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif

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

     DirectLink        *globals;
     FusionSkirmish    *globals_lock;

     FusionWorldShared *shared;
};

typedef struct {
     DirectLink         link;

     int                magic;

     pthread_mutex_t    lock;

     int                reactor_id;
     FusionReactor     *reactor;

     DirectLink        *reactions; /* reactor listeners attached to node  */
} ReactorNode;

/**************************************************************************************************/

static ReactorNode *lock_node      ( int                 reactor_id,
                                     bool                add_it,
                                     FusionReactor      *reactor, /* one of reactor and world must not be NULL */
                                     FusionWorld        *world );

static void         unlock_node    ( ReactorNode        *node );

static void         process_globals( FusionReactor      *reactor,
                                     const void         *msg_data,
                                     const ReactionFunc *globals );

/**************************************************************************************************/

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

     D_MAGIC_SET( reactor, FusionReactor );


     info.type = FT_REACTOR;
     info.id   = reactor->id;

     strncpy( info.name, name, sizeof(info.name) );

     ioctl( world->fusion_fd, FUSION_ENTRY_SET_INFO, &info );

     return reactor;
}

DirectResult
fusion_reactor_free( FusionReactor *reactor )
{
     FusionWorldShared *shared;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     shared = reactor->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "fusion_reactor_free( %p [%d] )\n", reactor, reactor->id );

     while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_DESTROY, &reactor->id )) {
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

     D_MAGIC_CLEAR( reactor );

     /* free shared reactor data */
     SHFREE( shared->main_pool, reactor );

     return DFB_OK;
}

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
fusion_reactor_attach( FusionReactor *reactor,
                       ReactionFunc   func,
                       void          *ctx,
                       Reaction      *reaction )
{
     ReactorNode *node;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_ASSERT( func != NULL );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_attach( %p [%d], func %p, ctx %p, reaction %p )\n",
                 reactor, reactor->id, func, ctx, reaction );

     node = lock_node( reactor->id, true, reactor, NULL );
     if (!node)
          return DFB_FUSION;

     while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_ATTACH, &reactor->id )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Reactor: invalid reactor\n" );
                    unlock_node( node );
                    return DFB_DESTROYED;
          }

          D_PERROR( "FUSION_REACTOR_ATTACH" );
          unlock_node( node );
          return DFB_FUSION;
     }

     /* fill out callback information */
     reaction->func     = func;
     reaction->ctx      = ctx;
     reaction->attached = true;

     /* prepend the reaction to the local reaction list */
     direct_list_prepend( &node->reactions, &reaction->link );

     unlock_node( node );

     return DFB_OK;
}

DirectResult
fusion_reactor_detach (FusionReactor *reactor,
                       Reaction      *reaction)
{
     ReactorNode *node;

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "fusion_reactor_detach( %p [%d], reaction %p ) <- func %p, ctx %p\n",
                 reactor, reactor->id, reaction, reaction->func, reaction->ctx );

     node = lock_node( reactor->id, false, reactor, NULL );
     if (!node) {
          D_BUG( "node not found" );
          return DFB_BUG;
     }

     D_ASSUME( reaction->attached );

     if (reaction->attached) {
          reaction->attached = false;

          direct_list_remove( &node->reactions, &reaction->link );

          while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_DETACH, &reactor->id )) {
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

    return fusion_reactor_sized_dispatch(reactor,
                                         msg_data,
                                         reactor->msg_size,
                                         self,
                                         globals);
}

DirectResult
fusion_reactor_sized_dispatch( FusionReactor      *reactor,
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
     if (self)
          _fusion_reactor_process_message( reactor->id, msg_data, reactor, NULL );

     /* Initialize dispatch data. */
     dispatch.reactor_id = reactor->id;
     dispatch.self       = false;
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

/**************************************************************************************************/

void
_fusion_reactor_free_all( FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Reactor, "_fusion_reactor_free_all() <- nodes %p\n", world->reactor_nodes );

     /* FIXME */

     /*
     DirectLink *l;

     pthread_mutex_lock( &nodes_lock );

     direct_list_foreach (l, nodes) {
          DirectLink  *next = l->next;
          ReactorNode *node = (ReactorNode*) l;

           D_FREE( node );

          l = next;
     }
     */

     world->reactor_nodes = NULL;

     /*
     pthread_mutex_unlock( &nodes_lock );
     */
}

void
_fusion_reactor_process_message( int reactor_id, const void *msg_data,
                                 FusionReactor *reactor, FusionWorld *world )
{
     DirectLink        *n;
     Reaction          *reaction;
     ReactorNode       *node;
     FusionWorldShared *shared = NULL;

     D_ASSERT( msg_data != NULL );
     D_ASSERT( reactor != NULL || world != NULL );

     D_DEBUG_AT( Fusion_Reactor,
                 "  _fusion_reactor_process_message( [%d], msg_data %p )\n", reactor_id, msg_data );

     /* Find the local counter part of the reactor. */
     node = lock_node( reactor_id, false, reactor, world );
     if (!node)
          return;

     D_MAGIC_ASSUME( node->reactor, FusionReactor );

     if (node->reactor->magic == D_MAGIC( "FusionReactor" )) {
          shared = node->reactor->shared;

          D_MAGIC_ASSERT( shared, FusionWorldShared );
     }

//     D_DEBUG_AT( Fusion_Reactor, "    -> node %p, reactor %p\n", node, node->reactor );

//     D_ASSUME( node->reactions != NULL );

     if (!node->reactions) {
//          D_DEBUG_AT( Fusion_Reactor, "    -> no local reactions!?!\n" );
          unlock_node( node );
          return;
     }

     direct_list_foreach_safe (reaction, n, node->reactions) {
          /* FIXME: don't cleanup asynchronously */
          if (!reaction->attached) {
/*               D_DEBUG_AT( Fusion_Reactor, "    -> cleaning up %p, func %p, ctx %p\n",
                           reaction, reaction->func, reaction->ctx );*/

               direct_list_remove( &node->reactions, &reaction->link );
               continue;
          }

          if (reaction->func( msg_data, reaction->ctx ) == RS_REMOVE) {
/*               D_DEBUG_AT( Fusion_Reactor, "    -> removing %p, func %p, ctx %p\n",
                           reaction, reaction->func, reaction->ctx );*/

               reaction->attached = false;

               direct_list_remove( &node->reactions, &reaction->link );

               if (shared) {
                    while (ioctl( _fusion_fd( shared ), FUSION_REACTOR_DETACH, &reactor_id )) {
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
     }

     unlock_node( node );
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
lock_node( int reactor_id, bool add_it, FusionReactor *reactor, FusionWorld *world )
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
          if (node->reactor_id == reactor_id) {
               pthread_mutex_lock( &node->lock );

               /* FIXME: Don't cleanup asynchronously. */
               if (!node->reactions && !add_it) {
//                    D_DEBUG_AT( Fusion_Reactor, "      -> cleaning up mine %p\n", node );

                    direct_list_remove( &world->reactor_nodes, &node->link );

                    pthread_mutex_unlock( &node->lock );
                    pthread_mutex_destroy( &node->lock );

                    D_FREE( node );

                    node = NULL;
               }
               else {
/*                    D_DEBUG_AT( Fusion_Reactor, "      -> found %p (%d reactions)\n",
                                node, direct_list_count_elements_EXPENSIVE( node->reactions ) );*/

                    D_ASSERT( node->reactor == reactor || reactor == NULL );
               }

               pthread_mutex_unlock( &world->reactor_nodes_lock );

               return node;
          }

          /* FIXME: Don't cleanup asynchronously. */
          if (!pthread_mutex_trylock( &node->lock )) {
               if (!node->reactions) {
//                    D_DEBUG_AT( Fusion_Reactor, "      -> cleaning up other %p\n", node );

                    direct_list_remove( &world->reactor_nodes, &node->link );

                    pthread_mutex_unlock( &node->lock );
                    pthread_mutex_destroy( &node->lock );

                    D_FREE( node );
               }
               else {
                    /*D_DEBUG_AT( Fusion_Reactor, "      -> keeping other %p (%d reactions)\n",
                                node, direct_list_count_elements_EXPENSIVE( node->reactions ) );*/

                    pthread_mutex_unlock( &node->lock );
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

          direct_util_recursive_pthread_mutex_init( &node->lock );


          pthread_mutex_lock( &node->lock );

          node->reactor_id = reactor_id;
          node->reactor    = reactor;

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

     pthread_mutex_unlock( &node->lock );
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

     D_UNIMPLEMENTED();

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
fusion_reactor_free (FusionReactor *reactor)
{
     D_ASSERT( reactor != NULL );

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

