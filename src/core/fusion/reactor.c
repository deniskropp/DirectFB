/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#define _GNU_SOURCE

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <pthread.h>

#ifndef FUSION_FAKE
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif

#include <core/thread.h>
#include <misc/mem.h>

#include "fusion_types.h"
#include "list.h"
#include "lock.h"
#include "shmalloc.h"
#include "reactor.h"

#include "fusion_internal.h"

#ifndef FUSION_FAKE

/***************************
 *  Internal declarations  *
 ***************************/

/*
 *
 */
struct _FusionReactor {
     int id;        /* reactor id                          */
     int msg_size;  /* size of each message                */

     FusionLink     *globals;
     FusionSkirmish  globals_lock;
};

typedef struct {
     FusionLink       link;

     pthread_mutex_t  lock;
     
     int              reactor_id;

     FusionLink      *reactions; /* reactor listeners attached to node  */
} ReactorNode;

/*
 * List of reactors with at least one local reaction attached.
 */
static FusionLink      *nodes      = NULL;
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

     DFB_ASSERT( msg_size > 0 );

     /* allocate shared reactor data */
     reactor = shcalloc (1, sizeof (FusionReactor));
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

          FPERROR ("FUSION_REACTOR_NEW\n");

          shfree( reactor );

          return NULL;
     }

     /* set the static message size, should we make dynamic? (TODO?) */
     reactor->msg_size = msg_size;

     fusion_skirmish_init( &reactor->globals_lock );

     return reactor;
}

FusionResult
fusion_reactor_attach (FusionReactor *reactor,
                       React          react,
                       void          *ctx,
                       Reaction      *reaction)
{
     ReactorNode *node;

     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( reaction != NULL );

     while (ioctl (_fusion_fd, FUSION_REACTOR_ATTACH, &reactor->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid reactor\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_REACTOR_ATTACH\n");
          
          return FUSION_FAILURE;
     }
     
     node = lock_node( reactor->id, true );
     if (!node) {
          while (ioctl (_fusion_fd, FUSION_REACTOR_DETACH, &reactor->id)) {
               switch (errno) {
                    case EINTR:
                         continue;
                    case EINVAL:
                         FERROR ("invalid reactor\n");
                         return FUSION_DESTROYED;
                    default:
                         break;
               }
               
               FPERROR ("FUSION_REACTOR_DETACH\n");

               return FUSION_FAILURE;
          }
     }
     
     /* fill out callback information */
     reaction->react    = react;
     reaction->ctx      = ctx;
     reaction->attached = true;

     /* prepend the reaction to the local reaction list */
     fusion_list_prepend (&node->reactions, &reaction->link);

     unlock_node( node );
     
     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_detach (FusionReactor *reactor,
                       Reaction      *reaction)
{
     ReactorNode *node;

     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( reaction != NULL );

     if (!reaction->attached) {
          FDEBUG( "fusion_reactor_detach() called on reaction that isn't attached\n" );
          return FUSION_SUCCESS;
     }

     node = lock_node( reactor->id, false );
     if (!node) {
          BUG( "node not found" );
          return FUSION_BUG;
     }
     
     if (reaction->attached) {
          reaction->attached = false;

          fusion_list_remove( &node->reactions, &reaction->link );

          while (ioctl (_fusion_fd, FUSION_REACTOR_DETACH, &reactor->id)) {
               switch (errno) {
                    case EINTR:
                         continue;
                    case EINVAL:
                         FERROR ("invalid reactor\n");
                         return FUSION_DESTROYED;
                    default:
                         break;
               }
               
               FPERROR ("FUSION_REACTOR_DETACH\n");

               return FUSION_FAILURE;
          }
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

     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( react_index >= 0 );
     DFB_ASSERT( reaction != NULL );

     ret = fusion_skirmish_prevail( &reactor->globals_lock );
     if (ret)
          return ret;
     
     /* fill out callback information */
     reaction->react_index = react_index;
     reaction->ctx         = ctx;
     reaction->attached    = true;

     /* prepend the reaction to the local reaction list */
     fusion_list_prepend (&reactor->globals, &reaction->link);

     fusion_skirmish_dismiss( &reactor->globals_lock );
     
     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_detach_global (FusionReactor  *reactor,
                              GlobalReaction *reaction)
{
     FusionResult ret;

     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( reaction != NULL );

     if (!reaction->attached) {
          FDEBUG( "fusion_reactor_detach_global() called "
                  "on reaction that isn't attached\n" );
          return FUSION_SUCCESS;
     }

     ret = fusion_skirmish_prevail( &reactor->globals_lock );
     if (ret)
          return ret;
     
     if (reaction->attached) {
          reaction->attached = false;

          fusion_list_remove( &reactor->globals, &reaction->link );
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

     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( msg_data != NULL );

     if (self)
          _fusion_reactor_process_message( reactor->id, msg_data );

     if (reactor->globals) {
          if (globals)
               process_globals( reactor, msg_data, globals );
          else
               FERROR( "global reactions exist but no "
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
                    FERROR ("invalid reactor\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_REACTOR_DISPATCH\n");
          
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_free (FusionReactor *reactor)
{
     DFB_ASSERT( reactor != NULL );

     fusion_skirmish_prevail( &reactor->globals_lock );
     fusion_skirmish_destroy( &reactor->globals_lock );
     
     while (ioctl (_fusion_fd, FUSION_REACTOR_DESTROY, &reactor->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid reactor\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }
          
          FPERROR ("FUSION_REACTOR_DESTROY\n");
          
          return FUSION_FAILURE;
     }
     
     /* free shared reactor data */
     shfree (reactor);

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
     FusionLink *l;

     pthread_mutex_lock( &nodes_lock );
     
     fusion_list_foreach (l, nodes) {
          FusionLink  *next = l->next;
          ReactorNode *node = (ReactorNode*) l;

           DFBFREE( node );

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
     FusionLink  *l;
     ReactorNode *node;

     DFB_ASSERT( msg_data != NULL );

     node = lock_node( reactor_id, false );
     if (!node) {
          //FDEBUG( "no node to dispatch message\n" );
          return;
     }
     
     if (!node->reactions) {
          FDEBUG( "node has no reactions\n" );
          unlock_node( node );
          return;
     }
     
     l = node->reactions;
     while (l) {
          FusionLink *next     = l->next;
          Reaction   *reaction = (Reaction*) l;
     
          if (reaction->attached) {
               if (reaction->react( msg_data, reaction->ctx ) == RS_REMOVE) {
                    reaction->attached = false;

                    fusion_list_remove( &node->reactions, &reaction->link );

                    while (ioctl (_fusion_fd, FUSION_REACTOR_DETACH, &reactor_id)) {
                         switch (errno) {
                              case EINTR:
                                   continue;
                              case EINVAL:
                                   FERROR ("invalid reactor\n");
                                   break;
                              default:
                                   FPERROR ("FUSION_REACTOR_DETACH\n");
                                   break;
                         }

                         break;
                    }
               }
          }
          else
               fusion_list_remove( &node->reactions, &reaction->link );
          
          l = next;
     }

     unlock_node( node );
}

static void
process_globals( FusionReactor *reactor,
                 const void    *msg_data,
                 const React   *globals )
{
     FusionLink *l;
     int         max_index = -1;

     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( msg_data != NULL );
     DFB_ASSERT( globals != NULL );

     while (globals[max_index+1]) {
          max_index++;
     }

     if (max_index < 0)
          return;
     
     if (fusion_skirmish_prevail( &reactor->globals_lock ))
          return;

     l = reactor->globals;
     while (l) {
          FusionLink     *next   = l->next;
          GlobalReaction *global = (GlobalReaction*) l;
     
          if (global->react_index < 0 || global->react_index > max_index) {
               FERROR( "global react index out of bounds (%d)\n",
                       global->react_index );
          }
          else {
               if (globals[ global->react_index ]( msg_data,
                                                   global->ctx ) == RS_REMOVE)
                    fusion_list_remove( &reactor->globals, &global->link );
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
     FusionLink *l;

     pthread_mutex_lock( &nodes_lock );

     l = nodes;
     while (l) {
          FusionLink  *next = l->next;
          ReactorNode *node = (ReactorNode*) l;

          if (node->reactor_id == reactor_id) {
               pthread_mutex_lock( &node->lock );
               pthread_mutex_unlock( &nodes_lock );
               return node;
          }

          /* FIXME: Don't cleanup asynchronously. */
          if (!pthread_mutex_trylock( &node->lock )) {
               if (!node->reactions) {
                    fusion_list_remove( &nodes, &node->link );

                    pthread_mutex_unlock( &node->lock );
                    pthread_mutex_destroy( &node->lock );

                    DFBFREE( node );
               }
               else
                    pthread_mutex_unlock( &node->lock );
          }
          
          l = next;
     }

     if (add) {
          pthread_mutexattr_t  attr;
          ReactorNode         *node = DFBCALLOC( 1, sizeof(ReactorNode) );

          pthread_mutexattr_init( &attr );
          pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );

          pthread_mutex_init( &node->lock, &attr );

          pthread_mutexattr_destroy( &attr );
          
          
          pthread_mutex_lock( &node->lock );
          
          node->reactor_id = reactor_id;

          fusion_list_prepend( &nodes, &node->link );
          
          pthread_mutex_unlock( &nodes_lock );

          return node;
     }
     
     pthread_mutex_unlock( &nodes_lock );

     return NULL;
}

static void
unlock_node( ReactorNode *node )
{
     DFB_ASSERT( node != NULL );

     pthread_mutex_unlock( &node->lock );
}

#else /* !FUSION_FAKE */

/***************************
 *  Internal declarations  *
 ***************************/

/*
 *
 */
struct _FusionReactor {
     FusionLink       *reactions; /* reactor listeners attached to node  */
     pthread_mutex_t   reactions_lock;
     
     FusionLink       *globals; /* global reactions attached to node  */
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
     pthread_mutexattr_t  attr;
     FusionReactor       *reactor;

     reactor = (FusionReactor*)DFBCALLOC( 1, sizeof(FusionReactor) );
     if (!reactor)
          return NULL;
     
     pthread_mutexattr_init( &attr );
     pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );

     pthread_mutex_init( &reactor->reactions_lock, &attr );
     pthread_mutex_init( &reactor->globals_lock, &attr );

     pthread_mutexattr_destroy( &attr );

     return reactor;
}

FusionResult
fusion_reactor_attach (FusionReactor *reactor,
                       React          react,
                       void          *ctx,
                       Reaction      *reaction)
{
     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( react != NULL );
     DFB_ASSERT( reaction != NULL );
     
     reaction->react = react;
     reaction->ctx   = ctx;

     pthread_mutex_lock( &reactor->reactions_lock );

     fusion_list_prepend( &reactor->reactions, &reaction->link );

     pthread_mutex_unlock( &reactor->reactions_lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_detach (FusionReactor *reactor,
                       Reaction      *reaction)
{
     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( reaction != NULL );
     
     pthread_mutex_lock( &reactor->reactions_lock );

     fusion_list_remove( &reactor->reactions, &reaction->link );

     pthread_mutex_unlock( &reactor->reactions_lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_attach_global (FusionReactor  *reactor,
                              int             react_index,
                              void           *ctx,
                              GlobalReaction *reaction)
{
     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( react_index >= 0 );
     DFB_ASSERT( reaction != NULL );

     reaction->react_index = react_index;
     reaction->ctx         = ctx;

     pthread_mutex_lock( &reactor->globals_lock );

     fusion_list_prepend( &reactor->globals, &reaction->link );

     pthread_mutex_unlock( &reactor->globals_lock );
     
     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_detach_global (FusionReactor  *reactor,
                              GlobalReaction *reaction)
{
     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( reaction != NULL );
     
     pthread_mutex_lock( &reactor->globals_lock );

     fusion_list_remove( &reactor->globals, &reaction->link );

     pthread_mutex_unlock( &reactor->globals_lock );
     
     return FUSION_SUCCESS;
}

FusionResult
fusion_reactor_dispatch (FusionReactor *reactor,
                         const void    *msg_data,
                         bool           self,
                         const React   *globals)
{
     FusionLink *l;

     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( msg_data != NULL );

     if (reactor->globals) {
          if (globals)
               process_globals( reactor, msg_data, globals );
          else
               FERROR( "global reactions exist but no "
                       "globals have been passed to dispatch()" );
     }
     
     if (!self)
          return FUSION_SUCCESS;

     pthread_mutex_lock( &reactor->reactions_lock );

     l = reactor->reactions;
     while (l) {
          FusionLink *next     = l->next;
          Reaction   *reaction = (Reaction*) l;

          switch (reaction->react( msg_data, reaction->ctx )) {
               case RS_REMOVE:
                    fusion_list_remove( &reactor->reactions, l );
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
     reactor->reactions = NULL;

     pthread_mutex_destroy( &reactor->reactions_lock );

     DFBFREE( reactor );

     return FUSION_SUCCESS;
}

/******************************************************************************/

static void
process_globals( FusionReactor *reactor,
                 const void    *msg_data,
                 const React   *globals )
{
     FusionLink *l;
     int         max_index = -1;

     DFB_ASSERT( reactor != NULL );
     DFB_ASSERT( msg_data != NULL );
     DFB_ASSERT( globals != NULL );

     while (globals[max_index+1]) {
          max_index++;
     }

     if (max_index < 0)
          return;
     
     pthread_mutex_lock( &reactor->globals_lock );

     l = reactor->globals;
     while (l) {
          FusionLink     *next   = l->next;
          GlobalReaction *global = (GlobalReaction*) l;
     
          if (global->react_index < 0 || global->react_index > max_index) {
               FERROR( "global react index out of bounds (%d)\n",
                       global->react_index );
          }
          else {
               if (globals[ global->react_index ]( msg_data,
                                                   global->ctx ) == RS_REMOVE)
                    fusion_list_remove( &reactor->globals, &global->link );
          }
          
          l = next;
     }

     pthread_mutex_unlock( &reactor->globals_lock );
}

#endif /* FUSION_FAKE */

