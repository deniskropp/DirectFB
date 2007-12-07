/*
   (c) Copyright 2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/call.h>
#include <fusion/fusion.h>
#include <fusion/hash.h>
#include <fusion/lock.h>
#include <fusion/shmalloc.h>

#include <coma/coma.h>
#include <coma/component.h>

D_DEBUG_DOMAIN( Coma_Component, "Coma/Component", "Coma Component" );

/**********************************************************************************************************************/

struct __COMA_ComaNotification {
     int                     magic;

     ComaNotificationID     id;
     ComaNotificationFlags  flags;

     ComaComponent         *component;

     ComaNotifyFunc         notify_func;
     void                  *notify_ctx;
};

/**********************************************************************************************************************/

static const ReactionFunc coma_component_globals[] = {
          NULL
};

static void
component_destructor( FusionObject *object, bool zombie, void *ctx )
{
     Coma          *coma      = ctx;
     ComaComponent *component = (ComaComponent*) object;

     D_MAGIC_ASSERT( component, ComaComponent );

     D_DEBUG_AT( Coma_Component, "%s( %p [%lu] )%s\n", __FUNCTION__, component, object->id, zombie ? " ZOMBIE!" : "" );

     _coma_internal_remove_component( coma, component );

     fusion_skirmish_destroy( &component->lock );

     fusion_call_destroy( &component->method_call );
     fusion_call_destroy( &component->notify_call );

     if (component->notifications) {
          D_ASSERT( component->num_notifications > 0 );

          SHFREE( component->shmpool, component->notifications );
     }
     else
          D_ASSERT( component->num_notifications == 0 );

     SHFREE( component->shmpool, component->name );

     D_MAGIC_CLEAR( component );

     fusion_object_destroy( object );
}

FusionObjectPool *
coma_component_pool_create( Coma *coma )
{
     return fusion_object_pool_create( "Component", sizeof(ComaComponent), sizeof(void*),
                                       component_destructor, coma, coma_world(coma) );
}

/**********************************************************************************************************************/

static FusionCallHandlerResult
method_call_handler( int           caller,
                     int           call_arg,
                     void         *call_ptr,
                     void         *ctx,
                     unsigned int  serial,
                     int          *ret_val )
{
     ComaComponent *component = ctx;

     D_MAGIC_ASSERT( component, ComaComponent );
     D_ASSERT( component->method_func != NULL );

     component->method_func( component->method_ctx, call_arg, call_ptr, serial );

     return FCHR_RETAIN;
}

static FusionCallHandlerResult
notify_call_handler( int           caller,
                     int           call_arg,
                     void         *call_ptr,
                     void         *ctx,
                     unsigned int  serial,
                     int          *ret_val )
{
     ComaNotification *notification;
     ComaComponent    *component = ctx;

     D_MAGIC_ASSERT( component, ComaComponent );

     D_ASSERT( call_arg >= 0 );
     D_ASSERT( call_arg < component->num_notifications );

     notification = &component->notifications[call_arg];

     D_MAGIC_ASSERT( notification, ComaNotification );

     if (notification->notify_func)
          notification->notify_func( notification->notify_ctx, call_arg, call_ptr );

     if (call_ptr && (notification->flags & CNF_DEALLOC_ARG))
          SHFREE( component->shmpool, call_ptr );

     return FCHR_RETURN;
}

/**********************************************************************************************************************/

DirectResult
coma_component_init( ComaComponent   *component,
                     Coma            *coma,
                     const char      *name,
                     ComaMethodFunc   func,
                     int              num_notifications,
                     void            *ctx )
{
     DirectResult  ret;
     FusionWorld  *world;

     D_ASSERT( component != NULL );
     D_ASSERT( coma != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( func != NULL );
     D_ASSERT( num_notifications >= 0 );

     D_DEBUG_AT( Coma_Component, "%s( %p, %p, '%s', %p, %d, %p )\n", __FUNCTION__,
                 component, coma, name, func, num_notifications, ctx );

     world = coma_world( coma );

     component->shmpool = coma_shmpool( coma );

     /* Initialize lock. */
     ret = fusion_skirmish_init( &component->lock, "Component", world );
     if (ret)
          return ret;

     /* Set name. */
     component->name = SHSTRDUP( component->shmpool, name );
     if (!component->name) {
          ret = D_OOSHM();
          goto error;
     }

     /* Create notification table. */
     if (num_notifications) {
          component->notifications = SHCALLOC( component->shmpool, num_notifications, sizeof(ComaNotification) );
          if (!component->notifications) {
               ret = D_OOSHM();
               goto error;
          }

          component->num_notifications = num_notifications;
     }

     /* Remember creator. */
     component->provider = fusion_id( world );

     /* Initialize calls. */
     fusion_call_init( &component->method_call, method_call_handler, component, world );
     fusion_call_init( &component->notify_call, notify_call_handler, component, world );

     /* Setup notification dispatch callback. */
     fusion_reactor_set_dispatch_callback( component->object.reactor, &component->notify_call, NULL );

     /* Change name of the reactor to something more specific than just "Component" from object pool. */
     fusion_reactor_set_name( component->object.reactor, name );

     /* Setup method invocation handler. */
     component->method_func = func;
     component->method_ctx  = ctx;

     D_MAGIC_SET( component, ComaComponent );

     return DFB_OK;


error:
     if (component->notifications)
          SHFREE( component->shmpool, component->notifications );

     if (component->name)
          SHFREE( component->shmpool, component->name );

     fusion_skirmish_destroy( &component->lock );

     return ret;
}

DirectResult
coma_component_lock( ComaComponent *component )
{
     D_MAGIC_ASSERT( component, ComaComponent );

     return fusion_skirmish_prevail( &component->lock );
}

DirectResult
coma_component_unlock( ComaComponent *component )
{
     D_MAGIC_ASSERT( component, ComaComponent );

     return fusion_skirmish_dismiss( &component->lock );
}

DirectResult
coma_component_init_notification( ComaComponent         *component,
                                  ComaNotificationID     id,
                                  ComaNotifyFunc         func,
                                  void                  *ctx,
                                  ComaNotificationFlags  flags )
{
     ComaNotification *notification;

     D_MAGIC_ASSERT( component, ComaComponent );

     D_DEBUG_AT( Coma_Component, "%s( %p, %lu - %p )\n", __FUNCTION__, component, id, func );

     if (id < 0 || id >= component->num_notifications)
          return DFB_LIMITEXCEEDED;

     notification = &component->notifications[id];
     if (notification->component) {
          D_MAGIC_ASSERT( notification, ComaNotification );
          return DFB_BUSY;
     }

     notification->id          = id;
     notification->flags       = flags;
     notification->component   = component;
     notification->notify_func = func;
     notification->notify_ctx  = ctx;

     D_MAGIC_SET( notification, ComaNotification );

     return DFB_OK;
}

DirectResult
coma_component_call( ComaComponent *component,
                     ComaMethodID   method,
                     void          *arg,
                     int           *ret_val )
{
     D_MAGIC_ASSERT( component, ComaComponent );

     D_DEBUG_AT( Coma_Component, "%s( %p, %lu - %p, %p )\n", __FUNCTION__, component, method, arg, ret_val );

     return fusion_call_execute( &component->method_call, FCEF_NONE, method, arg, ret_val );
}

DirectResult
coma_component_return( ComaComponent *component,
                       unsigned int   serial,
                       int            val )
{
     D_MAGIC_ASSERT( component, ComaComponent );

     D_DEBUG_AT( Coma_Component, "%s( %p, %u - %d )\n", __FUNCTION__, component, serial, val );

     return fusion_call_return( &component->method_call, serial, val );
}

DirectResult
coma_component_notify( ComaComponent                  *component,
                       ComaNotificationID              id,
                       void                           *arg )
{
     D_MAGIC_ASSERT( component, ComaComponent );

     D_DEBUG_AT( Coma_Component, "%s( %p [%lu], %lu - %p )\n", __FUNCTION__,
                 component, component->object.id, id, arg );

     D_ASSERT( id >= 0 );
     D_ASSERT( id < component->num_notifications );

     return coma_component_dispatch_channel( component, id, &arg, sizeof(void*), coma_component_globals );
}

