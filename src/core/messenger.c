/*
   (c) Copyright 2006-2007  directfb.org

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
#include <direct/messages.h>

#include <fusion/hash.h>
#include <fusion/lock.h>
#include <fusion/shmalloc.h>

#include <core/dale_core.h>
#include <core/messenger.h>
#include <core/messenger_port.h>

D_DEBUG_DOMAIN( DC_Mess, "Core/Messenger", "FusionDale Core Messenger" );

/**********************************************************************************************************************/

static void fd_messenger_notify( CoreMessenger                  *messenger,
                                 CoreMessengerNotificationFlags  flags,
                                 CoreMessengerDispatch          *dispatch );

/**********************************************************************************************************************/

static const ReactionFunc fd_messenger_globals[] = {
/* 0 */   _fd_messenger_port_messenger_listener,
          NULL
};

static void
messenger_destructor( FusionObject *object, bool zombie )
{
     CoreMessenger *messenger = (CoreMessenger*) object;

     D_MAGIC_ASSERT( messenger, CoreMessenger );

     D_DEBUG_AT( DC_Mess, "%s( %p [%lu] )%s\n", __FUNCTION__, messenger, object->id, zombie ? " ZOMBIE!" : "" );

     fusion_skirmish_destroy( &messenger->lock );

     D_ASSERT( fusion_hash_size( messenger->hash ) == 0 );

     fusion_hash_destroy( messenger->hash );

     D_MAGIC_CLEAR( messenger );

     fusion_object_destroy( object );
}

FusionObjectPool *
fd_messenger_pool_create( const FusionWorld *world )
{
     return fusion_object_pool_create( "Messenger", sizeof(CoreMessenger),
                                       sizeof(CoreMessengerNotification),
                                       messenger_destructor, world );
}

/**********************************************************************************************************************/

DirectResult
fd_messenger_create( CoreDale       *core,
                     CoreMessenger **ret_messenger )
{
     DirectResult   ret;
     CoreMessenger *messenger;

     D_ASSERT( core != NULL );
     D_ASSERT( ret_messenger != NULL );

     D_DEBUG_AT( DC_Mess, "%s( %p )\n", __FUNCTION__, core );

     /* Create messenger object. */
     messenger = fd_core_create_messenger( core );
     if (!messenger)
          return DFB_FUSION;

     /* Initialize private data. */
     messenger->shmpool = fd_core_shmpool( core );

     /* Initialize lock. */
     ret = fusion_skirmish_init( &messenger->lock, "Messenger", fd_core_world(core) );
     if (ret)
          goto error;

     /* Initialize hash. */
     ret = fusion_hash_create( messenger->shmpool, HASH_STRING, HASH_PTR, 11, &messenger->hash );
     if (ret) {
          D_DERROR( ret, "Core/Messenger: fusion_hash_create() failed!\n" );
          goto error_hash;
     }

     /* Activate messenger object. */
     fusion_object_activate( &messenger->object );

     D_MAGIC_SET( messenger, CoreMessenger );

     /* Return messenger object. */
     *ret_messenger = messenger;

     return DFB_OK;


error_hash:
     fusion_skirmish_destroy( &messenger->lock );

error:
     fusion_object_destroy( &messenger->object );

     return ret;
}

DirectResult
fd_messenger_create_event( CoreMessenger       *messenger,
                           const char          *name,
                           CoreMessengerEvent **ret_event )
{
     DirectResult        ret;
     CoreMessengerEvent *event;

     D_MAGIC_ASSERT( messenger, CoreMessenger );
     D_ASSERT( name != NULL );
     D_ASSERT( ret_event != NULL );

     /* Allocate event structure. */
     event = SHCALLOC( messenger->shmpool, 1, sizeof(CoreMessengerEvent) );
     if (!event)
          return D_OOSHM();

     /* Set back pointer. */
     event->messenger = messenger;

     /* Initialize event data. */
     event->id   = ++messenger->last_event;
     event->name = SHSTRDUP( messenger->shmpool, name );

     if (!event->name) {
          ret = D_OOSHM();
          goto error;
     }

     /* Insert event into hash table. */
     ret = fusion_hash_insert( messenger->hash, event->name, event );
     if (ret)
          goto error;

     /* Set magic. */
     D_MAGIC_SET( event, CoreMessengerEvent );

     /* Return new event. */
     *ret_event = event;

     return DFB_OK;


error:
     if (event->name)
          SHFREE( messenger->shmpool, event->name );

     SHFREE( messenger->shmpool, event );

     return ret;
}

DirectResult
fd_messenger_destroy_event( CoreMessenger      *messenger,
                            CoreMessengerEvent *event )
{
     DirectResult  ret;
     void         *old_key;
     void         *old_value;

     D_MAGIC_ASSERT( messenger, CoreMessenger );
     D_MAGIC_ASSERT( event, CoreMessengerEvent );

     /* Remove event from hash table. */
     ret = fusion_hash_remove( messenger->hash, event->name, &old_key, &old_value );
     if (ret) {
          D_BUG( "event '%s' [%lu] not found", event->name, event->id );
          return ret;
     }

     D_ASSERT( old_key == event->name );
     D_ASSERT( old_value == event );

     D_ASSERT( event->name != NULL );

     SHFREE( messenger->shmpool, event->name );

     D_MAGIC_CLEAR( event );

     SHFREE( messenger->shmpool, event );

     return ret;
}

DirectResult
fd_messenger_lookup_event( CoreMessenger       *messenger,
                           const char          *name,
                           CoreMessengerEvent **ret_event )
{
     CoreMessengerEvent *event;

     D_MAGIC_ASSERT( messenger, CoreMessenger );
     D_ASSERT( name != NULL );

     /* Lookup event in hash table. */
     event = fusion_hash_lookup( messenger->hash, name );
     if (!event)
          return DFB_ITEMNOTFOUND;

     D_MAGIC_ASSERT( event, CoreMessengerEvent );

     /* Can be NULL to just check for event existence. */
     if (ret_event)
          *ret_event = event;

     return DFB_OK;
}

DirectResult
fd_messenger_dispatch_event( CoreMessenger      *messenger,
                             CoreMessengerEvent *event,
                             int                 param,
                             void               *data_ptr,
                             unsigned int        data_size )
{
     CoreMessengerDispatch *dispatch;

     D_MAGIC_ASSERT( messenger, CoreMessenger );
     D_MAGIC_ASSERT( event, CoreMessengerEvent );
     D_ASSERT( event->id != FDM_EVENT_ID_NONE );
     D_ASSERT( data_ptr != NULL || data_size == 0 );

     /* Allocate dispatch structure. */
     dispatch = SHCALLOC( messenger->shmpool, 1, sizeof(CoreMessengerDispatch) );
     if (!dispatch)
          return D_OOSHM();

     /* Initialize dispatch structure. */
     dispatch->event_id  = event->id;
     dispatch->param     = param;
     dispatch->data      = data_ptr;
     dispatch->data_size = data_size;

     D_MAGIC_SET( dispatch, CoreMessengerDispatch );

     direct_list_append( &event->dispatches, &dispatch->link );

     fd_messenger_notify( messenger, CMNF_DISPATCH, dispatch );

     if (!dispatch->count) {
          direct_list_remove( &event->dispatches, &dispatch->link );

          D_MAGIC_CLEAR( dispatch );

          SHFREE( messenger->shmpool, dispatch );
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
fd_messenger_notify( CoreMessenger                  *messenger,
                     CoreMessengerNotificationFlags  flags,
                     CoreMessengerDispatch          *dispatch )
{
     CoreMessengerNotification notification;

     D_MAGIC_ASSERT( messenger, CoreMessenger );
     D_FLAGS_ASSERT( flags, CMNF_ALL );

     D_DEBUG_AT( DC_Mess, "%s( %p [%lu], 0x%08x )\n", __FUNCTION__, messenger, messenger->object.id, flags );

     notification.flags     = flags;
     notification.messenger = messenger;
     notification.dispatch  = dispatch;

     fd_messenger_dispatch( messenger, &notification, fd_messenger_globals );
}

