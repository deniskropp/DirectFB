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

#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/shmalloc.h>
#include <fusion/reactor.h>

#include "fusion_internal.h"

#if FUSION_BUILD_MULTI

/***************************
 *  Internal declarations  *
 ***************************/

/*
 *
 */
struct __Fusion_FusionReactor {
     int magic;

     int id;        /* reactor id                          */
     int msg_size;  /* size of each message                */

     DirectLink     *globals;
     FusionSkirmish  globals_lock;
};

typedef struct {
     DirectLink       link;

     pthread_mutex_t  lock;

     int              reactor_id;

     DirectLink      *reactions; /* reactor listeners attached to node  */
} ReactorNode;

/*
 * List of reactors with at least one local reaction attached.
 */
static DirectLink      *nodes      = NULL;
static pthread_mutex_t  nodes_lock = PTHREAD_MUTEX_INITIALIZER;

static ReactorNode *lock_node( int reactor_id, bool add );

static void         unlock_node( ReactorNode *node );

static void         process_globals( FusionReactor *reactor,
                                     const void    *msg_data,
                                     const React   *globals );

/****************
 *  Public API  *
 ****************/

FusionReactor *
fusion_reactor_new (int msg_size)
{
     FusionReactor *reactor;

     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( msg_size > 0 );

     /* allocate shared reactor data */
     reactor = SHCALLOC( 1, sizeof (FusionReactor) );
     if (!reactor)
          return NULL;

     /* create a new reactor */
     while (ioctl (_fusion_fd, FUSION_REACTOR_NEW, &reactor->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          D_PERROR ("FUSION_REACTOR_NEW");

          SHFREE( reactor );

          return NULL;
     }

     /* set the static message size, should we make dynamic? (TODO?) */
     reactor->msg_size = msg_size;

     fusion_skirmish_init( &reactor->globals_lock );

     D_MAGIC_SET( reactor, FusionReactor );

     return reactor;
}

FusionResult
fusion_reactor_attach (FusionReactor *reactor,
                       React          react,
                       void          *ctx,
                       Reaction      *reaction)
{
     ReactorNode *node;

     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( reactor != NULL );
     D_ASSERT( react != NULL );
     D_ASSERT( reaction != NULL );

     D_MAGIC_ASSERT( reactor, FusionReactor );

     while (ioctl (_fusion_fd, FUSION_REACTOR_ATTACH, &reactor->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reactor: invalid reactor\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REACTOR_ATTACH");

          return FUSION_FAILURE;
     }

     node = lock_node( reactor->id, true );
     if (!node) {
          while (ioctl (_fusion_fd, FUSION_REACTOR_DETACH, &reactor->id)) {
               switch (errno) {
                    case EINTR:
                         continue;
                    case EINVAL:
                         D_ERROR ("Fusion/Reactor: invalid reactor\n");
                         return FUSION_DESTROYED;
                    default:
                         break;
               }

               D_PERROR ("FUSION_REACTOR_DETACH");

               return FUSION_FAILURE;
          }

          return FUSION_FAILURE;
     }

     /* fill out callback information */
     reaction->react    = react;
     reaction->ctx      = ctx;
     reaction->attached = true;

     /* prepend the reaction to the local reaction list */
     direct_list_prepend (&node->reactions, &reaction->link);

     unlock_node( node );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_detach (FusionReactor *reactor,
                       Reaction      *reaction)
{
     ReactorNode *node;

     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( reactor != NULL );
     D_ASSERT( reaction != NULL );

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_ASSUME( reaction->attached );

     if (!reaction->attached)
          return FUSION_SUCCESS;

     node = lock_node( reactor->id, false );
     if (!node) {
          D_BUG( "node not found" );
          return FUSION_BUG;
     }

     if (reaction->attached) {
          reaction->attached = false;

          direct_list_remove( &node->reactions, &reaction->link );

          while (ioctl (_fusion_fd, FUSION_REACTOR_DETACH, &reactor->id)) {
               switch (errno) {
                    case EINTR:
                         continue;
                    case EINVAL:
                         D_ERROR ("Fusion/Reactor: invalid reactor\n");
                         return FUSION_DESTROYED;
                    default:
                         break;
               }

               D_PERROR ("FUSION_REACTOR_DETACH");

               return FUSION_FAILURE;
          }
     }
     else {
          D_DEBUG("Fusion/Reactor: reaction detached in the meantime\n");
     }

     unlock_node( node );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_attach_global (FusionReactor  *reactor,
                              int             react_index,
                              void           *ctx,
                              GlobalReaction *reaction)
{
     FusionResult ret;

     D_ASSERT( reactor != NULL );
     D_ASSERT( react_index >= 0 );
     D_ASSERT( reaction != NULL );

     D_MAGIC_ASSERT( reactor, FusionReactor );

     ret = fusion_skirmish_prevail( &reactor->globals_lock );
     if (ret)
          return ret;

     /* fill out callback information */
     reaction->react_index = react_index;
     reaction->ctx         = ctx;
     reaction->attached    = true;

     /* prepend the reaction to the global reaction list */
     direct_list_prepend (&reactor->globals, &reaction->link);

     fusion_skirmish_dismiss( &reactor->globals_lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_detach_global (FusionReactor  *reactor,
                              GlobalReaction *reaction)
{
     FusionResult ret;

     D_ASSERT( reactor != NULL );
     D_ASSERT( reaction != NULL );

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_ASSUME( reaction->attached );

     if (!reaction->attached)
          return FUSION_SUCCESS;

     ret = fusion_skirmish_prevail( &reactor->globals_lock );
     if (ret)
          return ret;

     if (reaction->attached) {
          reaction->attached = false;

          direct_list_remove( &reactor->globals, &reaction->link );
     }

     fusion_skirmish_dismiss( &reactor->globals_lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_dispatch (FusionReactor *reactor,
                         const void    *msg_data,
                         bool           self,
                         const React   *globals)
{
     FusionReactorDispatch dispatch;

     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( reactor != NULL );
     D_ASSERT( msg_data != NULL );

     D_MAGIC_ASSERT( reactor, FusionReactor );

     if (self)
          _fusion_reactor_process_message( reactor->id, msg_data );

     if (reactor->globals) {
          if (globals)
               process_globals( reactor, msg_data, globals );
          else
               D_ERROR( "Fusion/Reactor: global reactions exist but no "
                        "globals have been passed to dispatch()\n" );
     }

     dispatch.reactor_id = reactor->id;
     dispatch.self       = false;
     dispatch.msg_size   = reactor->msg_size;
     dispatch.msg_data   = msg_data;

     while (ioctl (_fusion_fd, FUSION_REACTOR_DISPATCH, &dispatch)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reactor: invalid reactor\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REACTOR_DISPATCH");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_free (FusionReactor *reactor)
{
     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( reactor != NULL );

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_MAGIC_CLEAR( reactor );

     fusion_skirmish_prevail( &reactor->globals_lock );
     fusion_skirmish_destroy( &reactor->globals_lock );

     while (ioctl (_fusion_fd, FUSION_REACTOR_DESTROY, &reactor->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    D_ERROR ("Fusion/Reactor: invalid reactor\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          D_PERROR ("FUSION_REACTOR_DESTROY");

          return FUSION_FAILURE;
     }

     /* free shared reactor data */
     SHFREE( reactor );

     return FUSION_SUCCESS;
}

/*******************************
 *  Fusion internal functions  *
 *******************************/

void
_fusion_reactor_free_all()
{
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

     nodes = NULL;

     /*
     pthread_mutex_unlock( &nodes_lock );
     */
}

void
_fusion_reactor_process_message( int reactor_id, const void *msg_data )
{
     DirectLink  *l;
     ReactorNode *node;

     D_ASSERT( _fusion_fd != -1 );
     D_ASSERT( msg_data != NULL );

     node = lock_node( reactor_id, false );
     if (!node) {
          //FDEBUG( "no node to dispatch message\n" );
          return;
     }

     if (!node->reactions) {
          D_DEBUG( "Fusion/Reactor: node has no reactions\n" );
          unlock_node( node );
          return;
     }

     l = node->reactions;
     while (l) {
          DirectLink *next     = l->next;
          Reaction   *reaction = (Reaction*) l;

          if (reaction->attached) {
               if (reaction->react( msg_data, reaction->ctx ) == RS_REMOVE) {
                    reaction->attached = false;

                    direct_list_remove( &node->reactions, &reaction->link );

                    while (ioctl (_fusion_fd, FUSION_REACTOR_DETACH, &reactor_id)) {
                         switch (errno) {
                              case EINTR:
                                   continue;
                              case EINVAL:
                                   D_ERROR ("Fusion/Reactor: invalid reactor\n");
                                   direct_trace_print_stacks();
                                   break;
                              default:
                                   D_PERROR ("FUSION_REACTOR_DETACH");
                                   break;
                         }

                         break;
                    }
               }
          }
          else
               direct_list_remove( &node->reactions, &reaction->link );

          l = next;
     }

     unlock_node( node );
}

static void
process_globals( FusionReactor *reactor,
                 const void    *msg_data,
                 const React   *globals )
{
     DirectLink *l;
     int         max_index = -1;

     D_ASSERT( reactor != NULL );
     D_ASSERT( msg_data != NULL );
     D_ASSERT( globals != NULL );

     D_MAGIC_ASSERT( reactor, FusionReactor );

     while (globals[max_index+1]) {
          max_index++;
     }

     if (max_index < 0)
          return;

     if (fusion_skirmish_prevail( &reactor->globals_lock ))
          return;

     l = reactor->globals;
     while (l) {
          DirectLink     *next   = l->next;
          GlobalReaction *global = (GlobalReaction*) l;

          if (global->react_index < 0 || global->react_index > max_index) {
               D_ERROR( "Fusion/Reactor: global react index out of bounds (%d)\n",
                        global->react_index );
          }
          else {
               if (globals[ global->react_index ]( msg_data,
                                                   global->ctx ) == RS_REMOVE)
                    direct_list_remove( &reactor->globals, &global->link );
          }

          l = next;
     }

     fusion_skirmish_dismiss( &reactor->globals_lock );
}

/*****************************
 *  File internal functions  *
 *****************************/

static ReactorNode *
lock_node( int reactor_id, bool add )
{
     DirectLink *l;

     pthread_mutex_lock( &nodes_lock );

     l = nodes;
     while (l) {
          DirectLink  *next = l->next;
          ReactorNode *node = (ReactorNode*) l;

          if (node->reactor_id == reactor_id) {
               pthread_mutex_lock( &node->lock );

               /* FIXME: Don't cleanup asynchronously. */
               if (!node->reactions && !add) {
                    direct_list_remove( &nodes, &node->link );
                    pthread_mutex_destroy( &node->lock );
                    D_FREE( node );
                    node = NULL;
               }

               pthread_mutex_unlock( &nodes_lock );

               return node;
          }

          /* FIXME: Don't cleanup asynchronously. */
          if (!pthread_mutex_trylock( &node->lock )) {
               if (!node->reactions) {
                    direct_list_remove( &nodes, &node->link );

                    pthread_mutex_unlock( &node->lock );
                    pthread_mutex_destroy( &node->lock );

                    D_FREE( node );
               }
               else
                    pthread_mutex_unlock( &node->lock );
          }

          l = next;
     }

     if (add) {
          ReactorNode *node = D_CALLOC( 1, sizeof(ReactorNode) );

          if (!node) {
               D_WARN( "out of memory" );
               return NULL;
          }

          fusion_pthread_recursive_mutex_init( &node->lock );


          pthread_mutex_lock( &node->lock );

          node->reactor_id = reactor_id;

          direct_list_prepend( &nodes, &node->link );

          pthread_mutex_unlock( &nodes_lock );

          return node;
     }

     pthread_mutex_unlock( &nodes_lock );

     return NULL;
}

static void
unlock_node( ReactorNode *node )
{
     D_ASSERT( node != NULL );

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
process_globals( FusionReactor *reactor,
                 const void    *msg_data,
                 const React   *globals );

/****************
 *  Public API  *
 ****************/

FusionReactor *
fusion_reactor_new (int msg_size)
{
     FusionReactor *reactor;

     D_ASSERT( msg_size > 0 );

     reactor = D_CALLOC( 1, sizeof(FusionReactor) );
     if (!reactor)
          return NULL;

     fusion_pthread_recursive_mutex_init( &reactor->reactions_lock );
     fusion_pthread_recursive_mutex_init( &reactor->globals_lock );

     return reactor;
}

FusionResult
fusion_reactor_attach (FusionReactor *reactor,
                       React          react,
                       void          *ctx,
                       Reaction      *reaction)
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( react != NULL );
     D_ASSERT( reaction != NULL );

     reaction->react = react;
     reaction->ctx   = ctx;

     pthread_mutex_lock( &reactor->reactions_lock );

     direct_list_prepend( &reactor->reactions, &reaction->link );

     pthread_mutex_unlock( &reactor->reactions_lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_detach (FusionReactor *reactor,
                       Reaction      *reaction)
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( reaction != NULL );

     pthread_mutex_lock( &reactor->reactions_lock );

     direct_list_remove( &reactor->reactions, &reaction->link );

     pthread_mutex_unlock( &reactor->reactions_lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_attach_global (FusionReactor  *reactor,
                              int             react_index,
                              void           *ctx,
                              GlobalReaction *reaction)
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( react_index >= 0 );
     D_ASSERT( reaction != NULL );

     reaction->react_index = react_index;
     reaction->ctx         = ctx;

     pthread_mutex_lock( &reactor->globals_lock );

     direct_list_prepend( &reactor->globals, &reaction->link );

     pthread_mutex_unlock( &reactor->globals_lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_detach_global (FusionReactor  *reactor,
                              GlobalReaction *reaction)
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( reaction != NULL );

     pthread_mutex_lock( &reactor->globals_lock );

     direct_list_remove( &reactor->globals, &reaction->link );

     pthread_mutex_unlock( &reactor->globals_lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_dispatch (FusionReactor *reactor,
                         const void    *msg_data,
                         bool           self,
                         const React   *globals)
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
          return FUSION_SUCCESS;

     pthread_mutex_lock( &reactor->reactions_lock );

     l = reactor->reactions;
     while (l) {
          DirectLink *next     = l->next;
          Reaction   *reaction = (Reaction*) l;

          switch (reaction->react( msg_data, reaction->ctx )) {
               case RS_REMOVE:
                    direct_list_remove( &reactor->reactions, l );
                    break;

               case RS_DROP:
                    pthread_mutex_unlock( &reactor->reactions_lock );
                    return FUSION_SUCCESS;

               default:
                    break;
          }

          l = next;
     }

     pthread_mutex_unlock( &reactor->reactions_lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_free (FusionReactor *reactor)
{
     D_ASSERT( reactor != NULL );

     reactor->reactions = NULL;

     pthread_mutex_destroy( &reactor->reactions_lock );

     D_FREE( reactor );

     return FUSION_SUCCESS;
}

/******************************************************************************/

static void
process_globals( FusionReactor *reactor,
                 const void    *msg_data,
                 const React   *globals )
{
     DirectLink *l;
     int         max_index = -1;

     D_ASSERT( reactor != NULL );
     D_ASSERT( msg_data != NULL );
     D_ASSERT( globals != NULL );

     while (globals[max_index+1]) {
          max_index++;
     }

     if (max_index < 0)
          return;

     pthread_mutex_lock( &reactor->globals_lock );

     l = reactor->globals;
     while (l) {
          DirectLink     *next   = l->next;
          GlobalReaction *global = (GlobalReaction*) l;

          if (global->react_index < 0 || global->react_index > max_index) {
               D_ERROR( "Fusion/Reactor: global react index out of bounds (%d)\n",
                        global->react_index );
          }
          else {
               if (globals[ global->react_index ]( msg_data,
                                                   global->ctx ) == RS_REMOVE)
                    direct_list_remove( &reactor->globals, &global->link );
          }

          l = next;
     }

     pthread_mutex_unlock( &reactor->globals_lock );
}

#endif

