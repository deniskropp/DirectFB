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
#include <direct/util.h>

#include <fusion/hash.h>
#include <fusion/lock.h>
#include <fusion/shmalloc.h>

#include <core/dale_core.h>
#include <core/messenger.h>
#include <core/messenger_port.h>

D_DEBUG_DOMAIN( DC_MPort, "Core/MessngPort", "FusionDale Core Messenger Port" );

/**********************************************************************************************************************/

typedef struct {
     DirectLink                link;

     int                       magic;

     CoreMessengerPort        *port;

     CoreMessengerEvent       *event;
     unsigned int              count;   /* number of registrations */

     DirectLink               *listeners;

     CoreMessengerDispatch    *next_dispatch;
} EventNode;

typedef struct {
     DirectLink                link;

     int                       magic;

     EventNode                *node;

     FDMessengerEventCallback  callback;
     void                     *context;

     FDMessengerListenerID     id;

     Reaction                  reaction;
} EventListener;

/**********************************************************************************************************************/

static void fd_messenger_port_notify( CoreMessengerPort                  *port,
                                      CoreMessengerPortNotificationFlags  flags,
                                      CoreMessengerDispatch              *dispatch );

/**********************************************************************************************************************/

static ReactionResult fd_messenger_port_reaction( const void *msg_data,
                                                  void       *ctx );

/**********************************************************************************************************************/

static void
purge_node( CoreMessengerPort *port,
            EventNode         *node )
{
     DirectResult        ret;
     DirectLink         *next;
     void               *old_value;
     CoreMessenger      *messenger;
     CoreMessengerEvent *event;
     EventListener      *listener;

     D_MAGIC_ASSERT( port, CoreMessengerPort );
     D_MAGIC_ASSERT( node, EventNode );
     D_MAGIC_ASSERT( node->event, CoreMessengerEvent );

     messenger = port->messenger;

     D_MAGIC_ASSERT( messenger, CoreMessenger );

     event = node->event;

     D_MAGIC_ASSERT( event, CoreMessengerEvent );

     /* Remove event node from hash table. FIXME: 2nd lookup */
     ret = fusion_hash_remove( port->nodes, (void*) node->event->id, NULL, &old_value );
     if (ret)
          D_BUG( "node for event id %lu not found", node->event->id );
     else
          D_ASSERT( old_value == node );

     D_ASSUME( node->count > 0 );

     if (node->count > 0) {
          ret = fd_messenger_lock( messenger );
          if (ret == DFB_OK) {
               CoreMessengerDispatch *dispatch;

               /* Clear pending dispatches. */
               direct_list_foreach_safe( dispatch, next, node->next_dispatch ) {
                    D_MAGIC_ASSERT( dispatch, CoreMessengerDispatch );

                    if (!--dispatch->count) {
                         if (dispatch->data)
                              SHFREE( messenger->shmpool, dispatch->data );

                         D_ASSUME( event->dispatches == &dispatch->link );

                         direct_list_remove( &event->dispatches, &dispatch->link );

                         D_MAGIC_CLEAR( dispatch );

                         SHFREE( messenger->shmpool, dispatch );
                    }
               }

               direct_list_remove( &event->nodes, &node->link );

               /* Decrease registration counter. */
               if (!/*--*/event->nodes)
                    fd_messenger_destroy_event( messenger, event );

               fd_messenger_unlock( messenger );
          }
          else
               D_BUG( "could not lock messenger" );

          /* Clear listeners. */
          direct_list_foreach_safe( listener, next, node->listeners ) {
               D_MAGIC_ASSERT( listener, EventListener );

               /* Remove listener from hash table. */
               ret = fusion_hash_remove( port->listeners, (void*) listener->id, NULL, &old_value );
               if (ret)
                    D_BUG( "listener id %lu not found", listener->id );
               else
                    D_ASSERT( old_value == listener );

               D_MAGIC_CLEAR( listener );

               SHFREE( messenger->shmpool, listener );
          }
     }

     D_MAGIC_CLEAR( node );

     SHFREE( messenger->shmpool, node );
}

/**********************************************************************************************************************/

static bool
node_iterator( FusionHash *hash,
               void       *key,
               void       *value,
               void       *ctx )
{
     EventNode         *node = value;
     CoreMessengerPort *port = ctx;

     D_MAGIC_ASSERT( node, EventNode );
     D_MAGIC_ASSERT( port, CoreMessengerPort );

     purge_node( port, node );

     return false;
}

static void
messenger_port_destructor( FusionObject *object, bool zombie )
{
     CoreMessengerPort *port = (CoreMessengerPort*) object;
     CoreMessenger     *messenger;

     D_MAGIC_ASSERT( port, CoreMessengerPort );

     D_DEBUG_AT( DC_MPort, "%s( %p [%lu] )%s\n", __FUNCTION__, port, object->id, zombie ? " ZOMBIE!" : "" );

     messenger = port->messenger;

     D_MAGIC_ASSERT( messenger, CoreMessenger );

     fd_messenger_detach_global( messenger, &port->reaction );

     fusion_skirmish_prevail( &port->lock );
     fusion_skirmish_destroy( &port->lock );

     fusion_hash_iterate( port->nodes, node_iterator, port );

     D_ASSUME( fusion_hash_size( port->nodes ) == 0 );
     fusion_hash_destroy( port->nodes );

     D_ASSUME( fusion_hash_size( port->listeners ) == 0 );
     fusion_hash_destroy( port->listeners );

     fd_messenger_unlink( &port->messenger );

     D_MAGIC_CLEAR( port );

     fusion_object_destroy( object );
}

FusionObjectPool *
fd_messenger_port_pool_create( const FusionWorld *world )
{
     return fusion_object_pool_create( "Messenger Port", sizeof(CoreMessengerPort),
                                       sizeof(CoreMessengerPortNotification),
                                       messenger_port_destructor, world );
}

/**********************************************************************************************************************/

DirectResult
fd_messenger_port_create( CoreDale           *core,
                          CoreMessenger      *messenger,
                          CoreMessengerPort **ret_port )
{
     DirectResult       ret;
     CoreMessengerPort *port;

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( messenger, CoreMessenger );
     D_ASSERT( ret_port != NULL );

     D_DEBUG_AT( DC_MPort, "%s( %p )\n", __FUNCTION__, core );

     /* Create messenger port object. */
     port = fd_core_create_messenger_port( core );
     if (!port)
          return DFB_FUSION;

     /* Set back pointer. */
     ret = fd_messenger_link( &port->messenger, messenger );
     if (ret)
          goto error;

     /* Attach global reaction to process all events. */
     ret = fd_messenger_attach_global( messenger, FD_MESSENGER_PORT_MESSENGER_LISTENER, port, &port->reaction );
     if (ret)
          goto error;

     /* Attach to the port to receive events that we listen to. */
     ret = fd_messenger_port_attach( port, fd_messenger_port_reaction, port, &port->local_reaction );
     if (ret)
          goto error_attach;

     /* Initialize lock. */
     ret = fusion_skirmish_init( &port->lock, "Messenger Port", fd_core_world(core) );
     if (ret)
          goto error_skirmish;

     /* Initialize event node hash. */
     ret = fusion_hash_create( messenger->shmpool, HASH_INT, HASH_PTR, 11, &port->nodes );
     if (ret) {
          D_DERROR( ret, "Core/MessngPort: fusion_hash_create() failed!\n" );
          goto error_hash;
     }

     /* Initialize listener hash. */
     ret = fusion_hash_create( messenger->shmpool, HASH_INT, HASH_PTR, 11, &port->listeners );
     if (ret) {
          D_DERROR( ret, "Core/MessngPort: fusion_hash_create() failed!\n" );
          goto error_hash2;
     }

     fusion_reactor_set_lock( port->object.reactor, &port->lock );

     /* Activate messenger port object. */
     fusion_object_activate( &port->object );

     D_MAGIC_SET( port, CoreMessengerPort );

     /* Return messenger port object. */
     *ret_port = port;

     return DFB_OK;


error_hash2:
     fusion_hash_destroy( port->nodes );

error_hash:
     fusion_skirmish_destroy( &port->lock );

error_skirmish:
     fd_messenger_detach( messenger, &port->local_reaction );

error_attach:
     fd_messenger_detach_global( messenger, &port->reaction );

error:
     if (port->messenger)
          fd_messenger_unlink( &port->messenger );

     fusion_object_destroy( &port->object );

     return ret;
}

DirectResult
fd_messenger_port_add_event( CoreMessengerPort  *port,
                             CoreMessengerEvent *event )
{
     DirectResult  ret;
     EventNode    *node;

     D_MAGIC_ASSERT( port, CoreMessengerPort );
     D_MAGIC_ASSERT( event, CoreMessengerEvent );

     /* Lock port. */
     ret = fusion_skirmish_prevail( &port->lock );
     if (ret)
          return ret;

     /* Try to lookup existing event node. */
     node = fusion_hash_lookup( port->nodes, (void*) event->id );
     if (node) {
          D_MAGIC_ASSERT( node, EventNode );
          D_ASSERT( node->count > 0 );

          /* Increase node counter. */
          node->count++;
     }
     else {
          CoreMessenger *messenger = port->messenger;

          D_MAGIC_ASSERT( messenger, CoreMessenger );

          /* Allocate node. */
          node = SHCALLOC( messenger->shmpool, 1, sizeof(EventNode) );
          if (!node) {
               ret = D_OOSHM();
               goto error;
          }

          /* Initialize node. */
          node->port  = port;
          node->event = event;
          node->count = 1;

          /* Insert node into hash table. */
          ret = fusion_hash_insert( port->nodes, (void*) event->id, node );
          if (ret) {
               SHFREE( messenger->shmpool, node );
               goto error;
          }

          D_MAGIC_SET( node, EventNode );

          direct_list_append( &event->nodes, &node->link );

          /* Increase event's node counter. */
          //event->nodes++;
     }

     /* Unlock port. */
     fusion_skirmish_dismiss( &port->lock );

     return DFB_OK;


error:
     fusion_skirmish_dismiss( &port->lock );

     return ret;
}

DirectResult
fd_messenger_port_remove_event( CoreMessengerPort  *port,
                                FDMessengerEventID  event_id )
{
     DirectResult   ret;
     EventNode     *node;
     CoreMessenger *messenger;

     D_MAGIC_ASSERT( port, CoreMessengerPort );
     D_ASSERT( event_id != FDM_EVENT_ID_NONE );

     messenger = port->messenger;

     D_MAGIC_ASSERT( messenger, CoreMessenger );

     /* Lock port. */
     ret = fusion_skirmish_prevail( &port->lock );
     if (ret)
          return ret;

     /* Lookup our event node. */
     node = fusion_hash_lookup( port->nodes, (void*) event_id );
     if (node) {
          D_MAGIC_ASSERT( node, EventNode );
          D_ASSERT( node->count > 0 );

          if (node->count > 1)
               node->count--;
          else
               purge_node( port, node );
     }
     else
          D_BUG( "node for event id %lu not found", event_id );

     /* Unlock port. */
     fusion_skirmish_dismiss( &port->lock );

     return DFB_OK;
}

DirectResult
fd_messenger_port_add_listener( CoreMessengerPort        *port,
                                FDMessengerEventID        event_id,
                                FDMessengerEventCallback  callback,
                                void                     *context,
                                FDMessengerListenerID    *ret_id )
{
     DirectResult   ret;
     CoreMessenger *messenger;
     EventNode     *node;
     EventListener *listener = NULL;

     D_MAGIC_ASSERT( port, CoreMessengerPort );
     D_ASSERT( event_id != FDM_EVENT_ID_NONE );
     D_ASSERT( callback != NULL );
     D_ASSERT( ret_id != NULL );

     messenger = port->messenger;

     D_MAGIC_ASSERT( messenger, CoreMessenger );

     /* Lock port. */
     ret = fusion_skirmish_prevail( &port->lock );
     if (ret)
          return ret;

     /* Lookup our event node. */
     node = fusion_hash_lookup( port->nodes, (void*) event_id );
     if (node) {
          D_MAGIC_ASSERT( node, EventNode );
          D_ASSERT( node->count > 0 );

          /* Allocate listener struct. */
          listener = SHCALLOC( messenger->shmpool, 1, sizeof(EventListener) );
          if (!listener) {
               ret = D_OOSHM();
               goto error;
          }

          /* Initialize listener. */
          listener->node     = node;
          listener->callback = callback;
          listener->context  = context;
          listener->id       = ++port->last_listener;

          /* Insert listener into hash table. */
          ret = fusion_hash_insert( port->listeners, (void*) listener->id, listener );
          if (ret)
               goto error;

          D_MAGIC_SET( listener, EventListener );

          /* Append listener to event node. */
          direct_list_append( &node->listeners, &listener->link );

          *ret_id = listener->id;
     }
     else
          D_BUG( "node for event id %lu not found", event_id );

     /* Unlock port. */
     fusion_skirmish_dismiss( &port->lock );

     return DFB_OK;


error:
     if (listener)
          SHFREE( messenger->shmpool, listener );

     fusion_skirmish_dismiss( &port->lock );

     return ret;
}

DirectResult
fd_messenger_port_remove_listener( CoreMessengerPort     *port,
                                   FDMessengerListenerID  listener_id )
{
     DirectResult   ret;
     void          *old_value;
     EventNode     *node;
     EventListener *listener;
     CoreMessenger *messenger;

     D_MAGIC_ASSERT( port, CoreMessengerPort );
     D_ASSERT( listener_id != FDM_LISTENER_ID_NONE );

     messenger = port->messenger;

     D_MAGIC_ASSERT( messenger, CoreMessenger );

     /* Lock port. */
     ret = fusion_skirmish_prevail( &port->lock );
     if (ret)
          return ret;

     /* Remove listener from hash table. */
     ret = fusion_hash_remove( port->listeners, (void*) listener_id, NULL, &old_value );
     if (ret) {
          D_BUG( "listener id %lu not found", listener_id );
          fusion_skirmish_dismiss( &port->lock );
          return ret;
     }

     listener = old_value;

     D_MAGIC_ASSERT( listener, EventListener );

     node = listener->node;

     D_MAGIC_ASSERT( node, EventNode );

     /* Remove listener from event node. */
     direct_list_remove( &node->listeners, &listener->link );

     D_MAGIC_CLEAR( listener );

     SHFREE( messenger->shmpool, listener );

     /* Unlock port. */
     fusion_skirmish_dismiss( &port->lock );

     return DFB_OK;
}

DirectResult
fd_messenger_port_enum_listeners( CoreMessengerPort      *port,
                                  FDMessengerEventID      event_id,
                                  CoreMPListenerCallback  callback,
                                  void                   *context )
{
     DirectResult  ret;
     EventNode    *node;

     D_MAGIC_ASSERT( port, CoreMessengerPort );
     D_ASSERT( event_id != FDM_EVENT_ID_NONE );
     D_ASSERT( callback != NULL );

     /* Lock port. */
     ret = fusion_skirmish_prevail( &port->lock );
     if (ret)
          return ret;

     /* Lookup our event node. */
     node = fusion_hash_lookup( port->nodes, (void*) event_id );
     if (node) {
          EventListener *listener;

          D_MAGIC_ASSERT( node, EventNode );
          D_ASSERT( node->count > 0 );

          /* Loop through listeners for the event. */
          direct_list_foreach( listener, node->listeners ) {
               D_MAGIC_ASSERT( listener, EventListener );
               D_ASSERT( listener->callback != NULL );

               /* Pass each listener and its context to the enumeration callback. */
               if (callback( port, listener->callback, listener->context, context ) == DFENUM_CANCEL)
                    break;
          }
     }
     else
          D_BUG( "node for event id %lu not found", event_id );

     /* Unlock port. */
     fusion_skirmish_dismiss( &port->lock );

     return DFB_OK;
}

/**********************************************************************************************************************/

DirectResult
fd_messenger_event_dispatch( CoreMessengerEvent *event,
                             int                 param,
                             void               *data_ptr,
                             unsigned int        data_size )
{
     CoreMessenger         *messenger;
     CoreMessengerDispatch *dispatch;
     EventNode             *node;

     D_MAGIC_ASSERT( event, CoreMessengerEvent );
     D_ASSERT( event->id != FDM_EVENT_ID_NONE );
     D_ASSERT( data_ptr != NULL || data_size == 0 );

     messenger = event->messenger;

     D_MAGIC_ASSERT( messenger, CoreMessenger );

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



     direct_list_foreach( node, event->nodes ) {
          CoreMessengerPort *port;

          D_MAGIC_ASSERT( node, EventNode );
          D_ASSERT( node->count > 0 );
          D_ASSERT( node->event == event );

          port = node->port;

          D_MAGIC_ASSERT( port, CoreMessengerPort );

          /* Lock port. */
/*          if (fusion_skirmish_prevail( &port->lock )) {
               D_BUG( "could not lock port" );
               continue;
          }
*/
          if (node->listeners) {
               dispatch->count++;

               if (!node->next_dispatch)
                    node->next_dispatch = dispatch;

               /* Dispatch event to reaction in the port's process. */
               fd_messenger_port_notify( node->port, CMPNF_EVENT, dispatch );
          }

          /* Unlock port. */
//          fusion_skirmish_dismiss( &port->lock );
     }



     if (!dispatch->count) {
          direct_list_remove( &event->dispatches, &dispatch->link );

          D_MAGIC_CLEAR( dispatch );

          SHFREE( messenger->shmpool, dispatch );
     }

     return DFB_OK;
}

DirectResult
fd_messenger_port_send_event( CoreMessengerPort  *port,
                              FDMessengerEventID  event_id,
                              int                 param,
                              void               *data_ptr,
                              unsigned int        data_size )
{
     DirectResult        ret;
     EventNode          *node;
     CoreMessenger      *messenger;
     CoreMessengerEvent *event;

     D_MAGIC_ASSERT( port, CoreMessengerPort );
     D_ASSERT( event_id != FDM_EVENT_ID_NONE );
     D_ASSERT( data_ptr != NULL || data_size == 0 );

     messenger = port->messenger;

     D_MAGIC_ASSERT( messenger, CoreMessenger );

     /* Lock port. */
     ret = fusion_skirmish_prevail( &port->lock );
     if (ret)
          return ret;

     /* Lookup our event node. */
     node = fusion_hash_lookup( port->nodes, (void*) event_id );
     if (!node) {
          D_BUG( "node for event id %lu not found", event_id );
          fusion_skirmish_dismiss( &port->lock );
          return DFB_BUG;
     }

     D_MAGIC_ASSERT( node, EventNode );
     D_ASSERT( node->count > 0 );

     event = node->event;

     D_MAGIC_ASSERT( event, CoreMessengerEvent );

     /* Dispatch via messenger. */
     //ret = fd_messenger_dispatch_event( messenger, event, param, data_ptr, data_size );

     ret = fd_messenger_event_dispatch( event, param, data_ptr, data_size );

     /* Unlock port. */
     fusion_skirmish_dismiss( &port->lock );

     return ret;
}

/**********************************************************************************************************************/

static ReactionResult
fd_messenger_port_reaction( const void *msg_data,
                            void       *ctx )
{
     DirectResult                         ret;
     const CoreMessengerPortNotification *notification = msg_data;
     CoreMessengerPort                   *port         = ctx;
     EventNode                           *node;
     EventListener                       *listener;
     CoreMessenger                       *messenger;
     CoreMessengerDispatch               *dispatch;
     CoreMessengerEvent                  *event;

     D_ASSERT( notification != NULL );
     D_ASSERT( notification->event_id != FDM_EVENT_ID_NONE );

     D_MAGIC_ASSERT( port, CoreMessengerPort );
     D_MAGIC_ASSERT( port->messenger, CoreMessenger );

     /* Lock port. */
     ret = fusion_skirmish_prevail( &port->lock );
     if (ret) {
          D_BUG( "could not lock port" );
          return RS_REMOVE;
     }

     /* Lookup our event node. */
     node = fusion_hash_lookup( port->nodes, (void*) notification->event_id );
     if (!node) {
          /* Probably purged while the message was pending. */
          D_WARN( "node for event id %lu not found", notification->event_id );
          fusion_skirmish_dismiss( &port->lock );
          return RS_OK;
     }

     D_MAGIC_ASSERT( node, EventNode );
     D_ASSERT( node->count > 0 );

     event = node->event;

     D_MAGIC_ASSERT( event, CoreMessengerEvent );

     messenger = event->messenger;

     D_MAGIC_ASSERT( messenger, CoreMessenger );

     D_ASSERT( node->next_dispatch == notification->dispatch );

     dispatch = node->next_dispatch;

     D_MAGIC_ASSERT( dispatch, CoreMessengerDispatch );
     D_ASSERT( direct_list_contains_element_EXPENSIVE( event->dispatches, &dispatch->link ) );

     /* Loop through listeners for the event. */
     direct_list_foreach( listener, node->listeners ) {
          D_MAGIC_ASSERT( listener, EventListener );
          D_ASSERT( listener->callback != NULL );

          /* Call each listener. */
          listener->callback( dispatch->event_id, dispatch->param, dispatch->data,
                              dispatch->data_size, listener->context );
     }

     /* FIXME: Temporarily increase counter to avoid intermittent purge after the following unlock. */
     node->count++;

     /* Unlock port. */
     fusion_skirmish_dismiss( &port->lock );


     /* Lock messenger. (has to happen without the port being locked!) */
     ret = fd_messenger_lock( messenger );
     if (ret) {
          D_BUG( "could not lock messenger" );
          return RS_REMOVE;
     }

     /* Lock port. */
     ret = fusion_skirmish_prevail( &port->lock );
     if (ret) {
          D_BUG( "could not lock port" );
          fd_messenger_unlock( messenger );
          return RS_REMOVE;
     }

     /* FIXME: Due to the lock break, some might fail if port has been destroyed.
        Probably remove this whole thing and use a reference counter per dispatch. */
     D_MAGIC_ASSERT( port, CoreMessengerPort );
     D_MAGIC_ASSERT( node, EventNode );
     D_ASSERT( node->count > 0 );
     D_MAGIC_ASSERT( event, CoreMessengerEvent );
     D_MAGIC_ASSERT( messenger, CoreMessenger );
     D_MAGIC_ASSERT( dispatch, CoreMessengerDispatch );
     D_ASSERT( direct_list_contains_element_EXPENSIVE( event->dispatches, &dispatch->link ) );

     node->next_dispatch = (CoreMessengerDispatch*) dispatch->link.next;

     if (node->count > 1) {
          node->count--;

          if (!--dispatch->count) {
               if (dispatch->data)
                    SHFREE( messenger->shmpool, dispatch->data );

               D_ASSUME( event->dispatches == &dispatch->link );

               direct_list_remove( &event->dispatches, &dispatch->link );

               D_MAGIC_CLEAR( dispatch );

               SHFREE( messenger->shmpool, dispatch );
          }
     }
     else
          purge_node( port, node );

     /* Unlock port. */
     fusion_skirmish_dismiss( &port->lock );

     /* Unlock messenger. */
     fd_messenger_unlock( messenger );

     return RS_OK;
}

/**********************************************************************************************************************/

static void
fd_messenger_port_notify( CoreMessengerPort                  *port,
                          CoreMessengerPortNotificationFlags  flags,
                          CoreMessengerDispatch              *dispatch )
{
     CoreMessengerPortNotification notification;

     D_MAGIC_ASSERT( port, CoreMessengerPort );
     D_FLAGS_ASSERT( flags, CMNF_ALL );

     D_DEBUG_AT( DC_MPort, "%s( %p [%lu], 0x%08x )\n", __FUNCTION__, port, port->object.id, flags );

     D_ASSERT( flags == CMPNF_EVENT );

     notification.flags      = flags;
     notification.port       = port;
     notification.event_id   = dispatch->event_id;
     notification.param      = dispatch->param;
     notification.data       = dispatch->data;
     notification.data_size  = dispatch->data_size;
     notification.dispatch   = dispatch;

     fd_messenger_port_dispatch( port, &notification, NULL /* no globals so far */ );
}

/**********************************************************************************************************************/

ReactionResult
_fd_messenger_port_messenger_listener( const void *msg_data,
                                       void       *ctx )
{
     const CoreMessengerNotification *notification = msg_data;
     CoreMessengerPort               *port         = ctx;
     CoreMessengerDispatch           *dispatch;
     EventNode                       *node;
     DirectResult                     ret;

     D_ASSERT( notification != NULL );
     D_MAGIC_ASSERT( port, CoreMessengerPort );

     D_ASSERT( notification->flags == CMNF_DISPATCH );

     dispatch = notification->dispatch;

     D_MAGIC_ASSERT( dispatch, CoreMessengerDispatch );

     /* Lock port. */
     ret = fusion_skirmish_prevail( &port->lock );
     if (ret)
          return RS_REMOVE;

     /* Lookup event node to check if port has any listeners for this event.
        TODO: Could be optimized by linking nodes into event and dispatch directly,
        i.e. without this global reaction, but requires different locking. */
     node = fusion_hash_lookup( port->nodes, (void*) dispatch->event_id );
     if (node) {
          D_MAGIC_ASSERT( node, EventNode );
          D_ASSERT( node->count > 0 );

          if (node->listeners) {
               dispatch->count++;

               if (!node->next_dispatch)
                    node->next_dispatch = dispatch;

               /* Dispatch event to reaction in the port's process. */
               fd_messenger_port_notify( port, CMPNF_EVENT, dispatch );
          }
     }
     
     /* Unlock port. */
     fusion_skirmish_dismiss( &port->lock );

     return RS_OK;
}

