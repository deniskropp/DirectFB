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

#include <pthread.h>

#include <core/fusion/object.h>
#include <core/fusion/shmalloc.h>

#include <core/thread.h>

#include "fusion_internal.h"

struct _FusionObjectPool {
     FusionSkirmish          lock;
     FusionLink             *objects;

     char                   *name;
     int                     object_size;
     int                     message_size;
     FusionObjectDestructor  destructor;

     CoreThread             *bone_collector;
     bool                    shutdown;
};

static void *bone_collector_loop( CoreThread *thread, void *arg );

FusionObjectPool *
fusion_object_pool_create( const char             *name,
                           int                     object_size,
                           int                     message_size,
                           FusionObjectDestructor  destructor )
{
     FusionObjectPool *pool;

     DFB_ASSERT( name != NULL );
     DFB_ASSERT( object_size >= sizeof(FusionObject) );
     DFB_ASSERT( message_size > 0 );
     DFB_ASSERT( destructor != NULL );

     /* Allocate shared memory for the pool. */
     pool = shcalloc( 1, sizeof(FusionObjectPool) );
     if (!pool)
          return NULL;

     /* Initialize the pool lock. */
     fusion_skirmish_init( &pool->lock );

     /* Fill information. */
     pool->name         = shstrdup( name );
     pool->object_size  = object_size;
     pool->message_size = message_size;
     pool->destructor   = destructor;

     /* Run bone collector thread. */
     pool->bone_collector = dfb_thread_create( CTT_CLEANUP,
                                               bone_collector_loop, pool );

     return pool;
}

FusionResult
fusion_object_pool_destroy( FusionObjectPool *pool )
{
     DFB_ASSERT( pool != NULL );

     /* Stop bone collector thread. */
     pool->shutdown = true;
     dfb_thread_join( pool->bone_collector );

     /* Destroy the pool lock. */
     fusion_skirmish_destroy( &pool->lock );

     /* Deallocate shared memory. */
     shfree( pool->name );
     shfree( pool );

     return FUSION_SUCCESS;
}

FusionResult
fusion_object_pool_enum   ( FusionObjectPool      *pool,
                            FusionObjectCallback   callback,
                            void                  *ctx )
{
     FusionLink *l;

     /* Lock the pool's object list. */
     fusion_skirmish_prevail( &pool->lock );

     l = pool->objects;
     while (l) {
          FusionObject *object = (FusionObject*) l;

          if (!callback( pool, object, ctx ))
               break;

          l = l->next;
     }

     /* Unlock the pool's object list. */
     fusion_skirmish_dismiss( &pool->lock );
     
     return FUSION_SUCCESS;
}

FusionObject *
fusion_object_create( FusionObjectPool *pool )
{
     FusionObject *object;

     DFB_ASSERT( pool != NULL );

     /* Allocate shared memory for the object. */
     object = shcalloc( 1, pool->object_size );
     if (!object)
          return NULL;

     /* Set "initializing" state. */
     object->state = FOS_INIT;

     /* Initialize the reference counter. */
     if (fusion_ref_init( &object->ref )) {
          shfree( object );
          return NULL;
     }

     /* Create a reactor for message dispatching. */
     object->reactor = fusion_reactor_new( pool->message_size );
     if (!object->reactor) {
          fusion_ref_destroy( &object->ref );
          shfree( object );
          return NULL;
     }

     /* Increase the object's reference counter. */
     fusion_ref_up( &object->ref, false );

     /* Lock the pool's object list. */
     fusion_skirmish_prevail( &pool->lock );

     FDEBUG("{%s} adding %p\n", pool->name, object);
     
     /* Set pool back pointer. */
     object->pool = pool;

     /* Add the object to the pool. */
     fusion_list_prepend( &pool->objects, &object->link );

     /* Unlock the pool's object list. */
     fusion_skirmish_dismiss( &pool->lock );
     
     return object;
}

FusionResult
fusion_object_activate( FusionObject *object )
{
     /* Set "active" state. */
     object->state = FOS_ACTIVE;
     
     return FUSION_SUCCESS;
}

FusionResult
fusion_object_attach( FusionObject     *object,
                      React             react,
                      void             *ctx,
                      Reaction         *reaction )
{
     return fusion_reactor_attach( object->reactor, react, ctx, reaction );
}

FusionResult
fusion_object_detach( FusionObject     *object,
                      Reaction         *reaction )
{
     return fusion_reactor_detach( object->reactor, reaction );
}

FusionResult
fusion_object_attach_global( FusionObject     *object,
                             int               react_index,
                             void             *ctx,
                             GlobalReaction   *reaction )
{
     return fusion_reactor_attach_global( object->reactor,
                                   react_index, ctx, reaction );
}

FusionResult
fusion_object_detach_global( FusionObject     *object,
                             GlobalReaction   *reaction )
{
     return fusion_reactor_detach_global( object->reactor, reaction );
}

FusionResult
fusion_object_dispatch( FusionObject     *object,
                        void             *message,
                        const React      *globals )
{
     return fusion_reactor_dispatch( object->reactor, message, true, globals );
}

FusionResult
fusion_object_ref( FusionObject     *object )
{
     return fusion_ref_up( &object->ref, false );
}

FusionResult
fusion_object_unref( FusionObject     *object )
{
     return fusion_ref_down( &object->ref, false );
}

FusionResult
fusion_object_link( FusionObject    **link,
                    FusionObject     *object )
{
     FusionResult ret;

     ret = fusion_ref_up( &object->ref, true );
     if (ret)
          return ret;

     *link = object;

     return FUSION_SUCCESS;
}

FusionResult
fusion_object_unlink( FusionObject     *object )
{
     return fusion_ref_down( &object->ref, true );
}

FusionResult
fusion_object_destroy( FusionObject     *object )
{
     DFB_ASSERT( object != NULL );

     /* Set "deinitializing" state. */
     object->state = FOS_DEINIT;
     
     /* Remove the object from the pool. */
     if (object->pool) {
          FusionObjectPool *pool = object->pool;

          /* Lock the pool's object list. */
          fusion_skirmish_prevail( &pool->lock );

          /* Remove the object from the pool. */
          if (object->pool) {
               object->pool = NULL;
               fusion_list_remove( &pool->objects, &object->link );
          }
          
          /* Unlock the pool's object list. */
          fusion_skirmish_dismiss( &pool->lock );
     }
     
     fusion_ref_destroy( &object->ref );

     fusion_reactor_free( object->reactor );

     shfree( object );

     return FUSION_SUCCESS;
}

/******************************************************************************/

static void *
bone_collector_loop( CoreThread *thread, void *arg )
{
     FusionLink       *l;
     FusionObjectPool *pool = (FusionObjectPool*) arg;

     while (!pool->shutdown) {
          usleep(100000);

          /* Lock the pool's object list. */
          fusion_skirmish_prevail( &pool->lock );

          l = pool->objects;
          while (l) {
               FusionObject *object = (FusionObject*) l;
               FusionLink   *next   = l->next;

               switch (fusion_ref_zero_trylock( &object->ref )) {
                    case FUSION_SUCCESS:
                         FDEBUG("{%s} dead object: %p\n", pool->name, object);

                         if (object->state == FOS_INIT) {
                              CAUTION( "won't destroy incomplete object, leaking memory" );
                              fusion_list_remove( &pool->objects, &object->link );
                              break;
                         }

                         /* Set "deinitializing" state. */
                         object->state = FOS_DEINIT;

                         /* Remove the object from the pool. */
                         object->pool = NULL;
                         fusion_list_remove( &pool->objects, &object->link );
                         
                         /* Call the destructor. */
                         pool->destructor( object, false );

                         break;

                    case FUSION_DESTROYED:
                         FDEBUG("already destroyed! removing %p from '%s'\n",
                                object, pool->name);

                         /* Remove the object from the pool. */
                         fusion_list_remove( &pool->objects, &object->link );

                    default:
                         break;
               }

               l = next;
          }

          /* Unlock the pool's object list. */
          fusion_skirmish_dismiss( &pool->lock );
     }

     /* shutdown */
     l = pool->objects;
     while (l) {
          int           refs;
          FusionObject *object = (FusionObject*) l;
          FusionLink   *next   = l->next;

          fusion_ref_stat( &object->ref, &refs );

          FDEBUG("{%s} undestroyed object: %p (refs: %d)\n",
                 pool->name, object, refs);
          
          /* Set "deinitializing" state. */
          object->state = FOS_DEINIT;
          
          /* Remove the object from the pool. */
          fusion_list_remove( &pool->objects, &object->link );
          object->pool = NULL;

          /* Call the destructor. */
          pool->destructor( object, true );

          l = next;
     }

     return NULL;
}

