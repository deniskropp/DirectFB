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

#include <core/coredefs.h>

#include "fusion_internal.h"

struct _FusionObjectPool {
     FusionSkirmish          lock;
     FusionLink             *objects;

     char                   *name;
     int                     object_size;
     int                     message_size;
     FusionObjectDestructor  destructor;

     pthread_t               bone_collector;
     bool                    shutdown;
};

static void *bone_collector_loop( void *arg );

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
     skirmish_init( &pool->lock );

     /* Fill information. */
     pool->name         = shstrdup( name );
     pool->object_size  = object_size;
     pool->message_size = message_size;
     pool->destructor   = destructor;

     /* Run bone collector thread. */
     pthread_create( &pool->bone_collector, NULL, bone_collector_loop, pool );

     return pool;
}

FusionResult
fusion_object_pool_destroy( FusionObjectPool *pool )
{
     DFB_ASSERT( pool != NULL );

     /* Stop bone collector thread. */
     pool->shutdown = true;
     pthread_join( pool->bone_collector, NULL );

     /* Destroy the pool lock. */
     skirmish_destroy( &pool->lock );

     /* Deallocate shared memory. */
     shfree( pool->name );
     shfree( pool );

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

     /* Initialize the reference counter. */
     if (fusion_ref_init( &object->ref )) {
          shfree( object );
          return NULL;
     }

     /* Create a reactor for message dispatching. */
     object->reactor = reactor_new( pool->message_size );
     if (!object->reactor) {
          fusion_ref_destroy( &object->ref );
          shfree( object );
          return NULL;
     }

     /* Increase the object's reference counter. */
     fusion_ref_up( &object->ref, false );

     /* Lock the pool's object list. */
     skirmish_prevail( &pool->lock );

     FDEBUG("adding %p to pool %p (%s)\n", object, pool, pool->name);
     
     /* Add the object to the pool. */
     fusion_list_prepend( &pool->objects, &object->link );

     /* Unlock the pool's object list. */
     skirmish_dismiss( &pool->lock );
     
     /* Set pool back pointer. */
     object->pool = pool;

     return object;
}

FusionResult
fusion_object_attach( FusionObject     *object,
                      React             react,
                      void             *ctx )
{
     return reactor_attach( object->reactor, react, ctx );
}

FusionResult
fusion_object_detach( FusionObject     *object,
                      React             react,
                      void             *ctx )
{
     return reactor_detach( object->reactor, react, ctx );
}

FusionResult
fusion_object_dispatch( FusionObject     *object,
                        void             *message )
{
     return reactor_dispatch( object->reactor, message, true );
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

     fusion_ref_destroy( &object->ref );

     reactor_free( object->reactor );

     shfree( object );

     return FUSION_SUCCESS;
}

/******************************************************************************/

static void *
bone_collector_loop( void *arg )
{
     FusionLink       *l;
     FusionObjectPool *pool = (FusionObjectPool*) arg;

     while (!pool->shutdown) {
          usleep(100000);

          /* Lock the pool's object list. */
          skirmish_prevail( &pool->lock );

          l = pool->objects;
          while (l) {
               FusionObject *object = (FusionObject*) l;
               FusionLink   *next   = l->next;

               switch (fusion_ref_zero_trylock( &object->ref )) {
                    case FUSION_SUCCESS:
                         FDEBUG("found dead object: %p in '%s'\n",
                                object, pool->name);

                         /* Remove the object from the pool. */
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
          skirmish_dismiss( &pool->lock );
     }

     /* shutdown */
     l = pool->objects;
     while (l) {
          FusionObject *object = (FusionObject*) l;
          FusionLink   *next   = l->next;

          FDEBUG("found undestroyed object in pool: %p (%s)\n",
                 object, pool->name);
          
          /* Remove the object from the pool. */
          fusion_list_remove( &pool->objects, &object->link );

          /* Call the destructor. */
          pool->destructor( object, true );

          l = next;
     }

     return NULL;
}

