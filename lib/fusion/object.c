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

#include <pthread.h>

#include <direct/messages.h>
#include <direct/thread.h>

#include <fusion/build.h>
#include <fusion/object.h>
#include <fusion/shmalloc.h>

#include "fusion_internal.h"

struct __Fusion_FusionObjectPool {
     FusionSkirmish          lock;
     DirectLink             *objects;
     int                     ids;

     char                   *name;
     int                     object_size;
     int                     message_size;
     FusionObjectDestructor  destructor;

     FusionCall              call;
};

static int
object_reference_watcher( int caller, int call_arg, void *call_ptr, void *ctx )
{
     DirectLink       *l;
     FusionObjectPool *pool = ctx;

     if (caller) {
          D_BUG( "call not from Fusion" );
          return 0;
     }

     /* Lock the pool. */
     if (fusion_skirmish_prevail( &pool->lock ))
          return 0;

     /* Lookup the object. */
     direct_list_foreach (l, pool->objects) {
          FusionObject *object = (FusionObject*) l;

          if (object->id == call_arg) {
               switch (fusion_ref_zero_trylock( &object->ref )) {
                    case FUSION_SUCCESS:
                         break;

                    case FUSION_DESTROYED:
                         D_BUG("object already destroyed");
                         direct_list_remove( &pool->objects, &object->link );
                         fusion_skirmish_dismiss( &pool->lock );
                         return 0;

                    case FUSION_INUSE:
                         D_BUG("object revived in the meantime");
                         /* fall through */

                    default:
                         fusion_skirmish_dismiss( &pool->lock );
                         return 0;
               }

               D_DEBUG("Fusion/ObjectPool: {%s} dead object: %p\n", pool->name, object);

               if (object->state == FOS_INIT) {
                    D_WARN( "won't destroy incomplete object, leaking memory" );
                    direct_list_remove( &pool->objects, &object->link );
                    fusion_skirmish_dismiss( &pool->lock );
                    return 0;
               }

               /* Set "deinitializing" state. */
               object->state = FOS_DEINIT;

               /* Remove the object from the pool. */
               object->pool = NULL;
               direct_list_remove( &pool->objects, &object->link );

               /* Call the destructor. */
               pool->destructor( object, false );

               /* Unlock the pool. */
               fusion_skirmish_dismiss( &pool->lock );

               return 0;
          }
     }

     D_BUG("object not found");

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return 0;
}

FusionObjectPool *
fusion_object_pool_create( const char             *name,
                           int                     object_size,
                           int                     message_size,
                           FusionObjectDestructor  destructor )
{
     FusionObjectPool *pool;

     D_ASSERT( name != NULL );
     D_ASSERT( object_size >= sizeof(FusionObject) );
     D_ASSERT( message_size > 0 );
     D_ASSERT( destructor != NULL );

     /* Allocate shared memory for the pool. */
     pool = SHCALLOC( 1, sizeof(FusionObjectPool) );
     if (!pool)
          return NULL;

     /* Initialize the pool lock. */
     fusion_skirmish_init( &pool->lock );

     /* Fill information. */
     pool->name         = SHSTRDUP( name );
     pool->object_size  = object_size;
     pool->message_size = message_size;
     pool->destructor   = destructor;

     /* Destruction call from Fusion. */
     fusion_call_init( &pool->call, object_reference_watcher, pool );

     return pool;
}

FusionResult
fusion_object_pool_destroy( FusionObjectPool *pool )
{
     DirectLink *l;

     D_ASSERT( pool != NULL );

     /* Wait for processing of pending messages. */
     fusion_sync();

     /* Lock the pool. */
     if (fusion_skirmish_prevail( &pool->lock ))
          return FUSION_FAILURE;

     /* Destroy the call. */
     fusion_call_destroy( &pool->call );

     /* Destroy zombies */
     l = pool->objects;
     while (l) {
          int           refs;
          FusionObject *object = (FusionObject*) l;
          DirectLink   *next   = l->next;

          fusion_ref_stat( &object->ref, &refs );

          D_DEBUG( "Fusion/ObjectPool: {%s} undestroyed object: %p (refs: %d)\n",
                   pool->name, object, refs );

          /* Set "deinitializing" state. */
          object->state = FOS_DEINIT;

          /* Remove the object from the pool. */
          direct_list_remove( &pool->objects, &object->link );
          object->pool = NULL;

          /* Call the destructor. */
          pool->destructor( object, refs > 0 );

          l = next;
     }

     /* Destroy the pool lock. */
     fusion_skirmish_destroy( &pool->lock );

     /* Deallocate shared memory. */
     SHFREE( pool->name );
     SHFREE( pool );

     return FUSION_SUCCESS;
}

FusionResult
fusion_object_pool_enum   ( FusionObjectPool      *pool,
                            FusionObjectCallback   callback,
                            void                  *ctx )
{
     DirectLink *l;

     /* Lock the pool. */
     if (fusion_skirmish_prevail( &pool->lock ))
          return FUSION_FAILURE;

     l = pool->objects;
     while (l) {
          FusionObject *object = (FusionObject*) l;

          if (!callback( pool, object, ctx ))
               break;

          l = l->next;
     }

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return FUSION_SUCCESS;
}

FusionObject *
fusion_object_create( FusionObjectPool *pool )
{
     FusionObject *object;

     D_ASSERT( pool != NULL );

     /* Lock the pool. */
     if (fusion_skirmish_prevail( &pool->lock ))
          return NULL;

     /* Allocate shared memory for the object. */
     object = SHCALLOC( 1, pool->object_size );
     if (!object) {
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Set "initializing" state. */
     object->state = FOS_INIT;

     /* Set object id. */
     object->id = pool->ids++;

     /* Initialize the reference counter. */
     if (fusion_ref_init( &object->ref )) {
          SHFREE( object );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Increase the object's reference counter. */
     fusion_ref_up( &object->ref, false );

     /* Install handler for automatic destruction. */
     if (fusion_ref_watch( &object->ref, &pool->call, object->id )) {
          fusion_ref_destroy( &object->ref );
          SHFREE( object );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Create a reactor for message dispatching. */
     object->reactor = fusion_reactor_new( pool->message_size );
     if (!object->reactor) {
          fusion_ref_destroy( &object->ref );
          SHFREE( object );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Set pool back pointer. */
     object->pool = pool;

     /* Add the object to the pool. */
     direct_list_prepend( &pool->objects, &object->link );

#if FUSION_BUILD_MULTI
     D_DEBUG("Fusion/ObjectPool: {%s} added %p with ref 0x%08x\n", pool->name, object, object->ref.id);
#else
     D_DEBUG("Fusion/ObjectPool: {%s} added %p\n", pool->name, object);
#endif

     D_MAGIC_SET( object, FusionObject );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return object;
}

FusionResult
fusion_object_activate( FusionObject *object )
{
     D_MAGIC_ASSERT( object, FusionObject );

     /* Set "active" state. */
     object->state = FOS_ACTIVE;

     return FUSION_SUCCESS;
}

FusionResult
fusion_object_destroy( FusionObject     *object )
{
     D_ASSERT( object != NULL );

     D_MAGIC_CLEAR( object );

     /* Set "deinitializing" state. */
     object->state = FOS_DEINIT;

     /* Remove the object from the pool. */
     if (object->pool) {
          FusionObjectPool *pool = object->pool;

          /* Lock the pool. */
          if (fusion_skirmish_prevail( &pool->lock ))
               return FUSION_FAILURE;

          /* Remove the object from the pool. */
          if (object->pool) {
               object->pool = NULL;
               direct_list_remove( &pool->objects, &object->link );
          }

          /* Unlock the pool. */
          fusion_skirmish_dismiss( &pool->lock );
     }

     fusion_ref_destroy( &object->ref );

     fusion_reactor_free( object->reactor );

     SHFREE( object );

     return FUSION_SUCCESS;
}

