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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/shmalloc.h>
#include <fusion/vector.h>

#include <core/core.h>
#include <core/input.h>
#include <core/layers_internal.h>
#include <core/windows_internal.h>

#include <misc/util.h>

#include <unique/device.h>
#include <unique/internal.h>


#define MAX_CLASSES 16


D_DEBUG_DOMAIN( UniQuE_Device, "UniQuE/Device", "UniQuE's Devices" );


static const ReactionFunc unique_device_globals[] = {
     _unique_input_switch_device_listener,
     _unique_cursor_device_listener,
     NULL
};

/**************************************************************************************************/

typedef struct {
     DirectLink       link;

     int              magic;

     DFBInputDeviceID source;

     GlobalReaction   reaction;
} DeviceConnection;

/**************************************************************************************************/

static void purge_connection( UniqueDevice     *device,
                              DeviceConnection *connection );

/**************************************************************************************************/

static const UniqueDeviceClass *classes[MAX_CLASSES] = { NULL };

static pthread_mutex_t          classes_lock  = PTHREAD_MUTEX_INITIALIZER;
static int                      classes_count = 0;

/**************************************************************************************************/

DFBResult
unique_device_class_register( const UniqueDeviceClass *clazz,
                              UniqueDeviceClassID     *ret_id )
{
     int i;

     D_DEBUG_AT( UniQuE_Device, "unique_device_class_register( %p )\n", clazz );

     D_ASSERT( clazz != NULL );
     D_ASSERT( ret_id != NULL );

     pthread_mutex_lock( &classes_lock );

     if (classes_count == MAX_CLASSES) {
          D_WARN( "too many classes" );
          pthread_mutex_unlock( &classes_lock );
          return DFB_LIMITEXCEEDED;
     }

     classes_count++;

     for (i=0; i<MAX_CLASSES; i++) {
          if (!classes[i]) {
               classes[i] = clazz;
               break;
          }
     }

     D_DEBUG_AT( UniQuE_Device, "    -> New class ID is %d.\n", i );

     D_ASSERT( i < MAX_CLASSES );

     *ret_id = i;

     pthread_mutex_unlock( &classes_lock );

     return DFB_OK;
}

DFBResult
unique_device_class_unregister( UniqueDeviceClassID id )
{
     D_DEBUG_AT( UniQuE_Device, "unique_device_class_unregister( %d )\n", id );

     pthread_mutex_lock( &classes_lock );

     D_ASSERT( id >= 0 );
     D_ASSERT( id < MAX_CLASSES );
     D_ASSERT( classes[id] != NULL );

     classes[id] = NULL;

     classes_count--;

     pthread_mutex_unlock( &classes_lock );

     return DFB_OK;
}

/**************************************************************************************************/

DFBResult
unique_device_create( CoreDFB              *core,
                      UniqueContext        *context,
                      UniqueDeviceClassID   class_id,
                      void                 *ctx,
                      UniqueDevice        **ret_device )
{
     DFBResult                ret;
     UniqueDevice            *device;
     const UniqueDeviceClass *clazz;

     D_DEBUG_AT( UniQuE_Device, "unique_device_create( class %d )\n", class_id );

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( class_id >= 0 );
     D_ASSERT( class_id < MAX_CLASSES );

     clazz = classes[class_id];

     D_ASSERT( clazz != NULL );
     D_ASSERT( ret_device != NULL );

     /* Allocate device data. */
     device = SHCALLOC( context->shmpool, 1, sizeof(UniqueDevice) );
     if (!device) {
          D_WARN( "out of (shared) memory" );
          return D_OOSHM();
     }

     /* Initialize device data. */
     device->context = context;
     device->clazz   = class_id;
     device->ctx     = ctx;

     /* Allocate private device data. */
     if (clazz->data_size) {
          device->data = SHCALLOC( context->shmpool, 1, clazz->data_size );
          if (!device->data) {
               ret = D_OOSHM();
               goto error;
          }
     }

     /* Create reactor for dispatching generated events. */
     device->reactor = fusion_reactor_new( sizeof(UniqueInputEvent),
                                           "UniQuE Device", dfb_core_world(core) );
     if (!device->reactor) {
          ret = DFB_FUSION;
          goto error;
     }

     fusion_reactor_set_lock( device->reactor, &context->stack->context->lock );

     D_MAGIC_SET( device, UniqueDevice );

     if (clazz->Initialize) {
          ret = clazz->Initialize( device, device->data, device->ctx );
          if (ret) {
               D_MAGIC_CLEAR( device );
               goto error;
          }
     }

     D_DEBUG_AT( UniQuE_Device, "  -> device created (%p)\n", device );

     *ret_device = device;

     return DFB_OK;


error:
     if (device->reactor)
          fusion_reactor_free( device->reactor );

     if (device->data)
          SHFREE( context->shmpool, device->data );

     SHFREE( context->shmpool, device );

     return ret;
}

DFBResult
unique_device_destroy( UniqueDevice *device )
{
     DirectLink              *n;
     DeviceConnection        *connection;
     UniqueContext           *context;
     const UniqueDeviceClass *clazz;

     D_MAGIC_ASSERT( device, UniqueDevice );

     context = device->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( device->clazz >= 0 );
     D_ASSERT( device->clazz < MAX_CLASSES );

     clazz = classes[device->clazz];

     D_ASSERT( clazz != NULL );

     D_DEBUG_AT( UniQuE_Device, "unique_device_destroy( %p )\n", device );

     direct_list_foreach_safe (connection, n, device->connections) {
          D_MAGIC_ASSERT( connection, DeviceConnection );

          purge_connection( device, connection );
     }

     D_ASSERT( device->connections == NULL );

     if (clazz->Shutdown)
          clazz->Shutdown( device, device->data, device->ctx );

     fusion_reactor_free( device->reactor );

     if (device->data)
          SHFREE( context->shmpool, device->data );

     D_MAGIC_CLEAR( device );

     SHFREE( context->shmpool, device );

     return DFB_OK;
}

DFBResult
unique_device_connect( UniqueDevice    *device,
                       CoreInputDevice *source )
{
     DFBResult         ret;
     WMShared         *shared;
     UniqueContext    *context;
     DeviceConnection *connection;

     D_MAGIC_ASSERT( device, UniqueDevice );

     D_ASSERT( source != NULL );

     D_ASSERT( device->clazz >= 0 );
     D_ASSERT( device->clazz < MAX_CLASSES );
     D_ASSERT( classes[device->clazz] != NULL );

     context = device->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     shared = context->shared;

     D_MAGIC_ASSERT( shared, WMShared );

     D_DEBUG_AT( UniQuE_Device, "unique_device_connect( %p, %p, ID %d )\n",
                 device, source, dfb_input_device_id( source ) );

     /* Allocate connection structure. */
     connection = SHCALLOC( context->shmpool, 1, sizeof(DeviceConnection) );
     if (!connection)
          return D_OOSHM();

     /* Initialize connection structure. */
     connection->source = dfb_input_device_id( source );

     /* Attach global reaction for processing events. */
     ret = dfb_input_attach_global( source, shared->device_listener,
                                    device, &connection->reaction );
     if (ret) {
          SHFREE( context->shmpool, connection );
          return ret;
     }

     /* Add the new connection to the list. */
     direct_list_append( &device->connections, &connection->link );

     D_MAGIC_SET( connection, DeviceConnection );

     if (classes[device->clazz]->Connected)
          classes[device->clazz]->Connected( device, device->data, device->ctx, source );

     return DFB_OK;
}

DFBResult
unique_device_disconnect( UniqueDevice    *device,
                          CoreInputDevice *source )
{
     DFBInputDeviceID  source_id;
     DeviceConnection *connection;

     D_MAGIC_ASSERT( device, UniqueDevice );

     D_ASSERT( source != NULL );

     D_ASSERT( device->clazz >= 0 );
     D_ASSERT( device->clazz < MAX_CLASSES );
     D_ASSERT( classes[device->clazz] != NULL );

     source_id = dfb_input_device_id( source );

     direct_list_foreach (connection, device->connections) {
          D_MAGIC_ASSERT( connection, DeviceConnection );

          if (connection->source == source_id)
               break;
     }

     if (!connection) {
          D_WARN( "source not found amoung connections" );
          return DFB_ITEMNOTFOUND;
     }

     purge_connection( device, connection );

     return DFB_OK;
}

DFBResult
unique_device_attach( UniqueDevice *device,
                      ReactionFunc  func,
                      void         *ctx,
                      Reaction     *reaction )
{
     D_MAGIC_ASSERT( device, UniqueDevice );

     return fusion_reactor_attach( device->reactor, func, ctx, reaction );
}

DFBResult
unique_device_detach( UniqueDevice *device,
                      Reaction     *reaction )
{
     D_MAGIC_ASSERT( device, UniqueDevice );

     return fusion_reactor_detach( device->reactor, reaction );
}

DFBResult
unique_device_attach_global( UniqueDevice   *device,
                             int             index,
                             void           *ctx,
                             GlobalReaction *reaction )
{
     D_MAGIC_ASSERT( device, UniqueDevice );

     return fusion_reactor_attach_global( device->reactor, index, ctx, reaction );
}

DFBResult
unique_device_detach_global( UniqueDevice   *device,
                             GlobalReaction *reaction )
{
     D_MAGIC_ASSERT( device, UniqueDevice );

     return fusion_reactor_detach_global( device->reactor, reaction );
}

DFBResult
unique_device_dispatch( UniqueDevice           *device,
                        const UniqueInputEvent *event )
{
     D_MAGIC_ASSERT( device, UniqueDevice );

     return fusion_reactor_dispatch( device->reactor, event, true, unique_device_globals );
}

bool
unique_device_filter ( UniqueDeviceClassID     class_id,
                       const UniqueInputEvent *event,
                       const UniqueInputEvent *filter )
{
     const UniqueDeviceClass *clazz;

     D_ASSERT( class_id >= 0 );
     D_ASSERT( class_id < MAX_CLASSES );

     D_ASSERT( event != NULL );
     D_ASSERT( filter != NULL );

     clazz = classes[class_id];

     D_ASSERT( clazz != NULL );

     if (clazz->FilterEvent)
          return clazz->FilterEvent( event, filter );

     return false;
}

/**************************************************************************************************/

ReactionResult
_unique_device_listener( const void *msg_data,
                         void       *ctx )
{
     const DFBInputEvent *event  = msg_data;
     UniqueDevice        *device = ctx;

     D_ASSERT( event != NULL );

     D_MAGIC_ASSERT( device, UniqueDevice );

     D_ASSERT( device->clazz >= 0 );
     D_ASSERT( device->clazz < MAX_CLASSES );
     D_ASSERT( classes[device->clazz] != NULL );
     D_ASSERT( classes[device->clazz]->ProcessEvent != NULL );

     classes[device->clazz]->ProcessEvent( device, device->data, device->ctx, event );

     return RS_OK;
}

/**************************************************************************************************/

static void
purge_connection( UniqueDevice     *device,
                  DeviceConnection *connection )
{
     CoreInputDevice *source;
     UniqueContext   *context;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( connection, DeviceConnection );

     context = device->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     source = dfb_input_device_at( connection->source );

     D_ASSERT( source != NULL );

     /* Detach global reaction for processing events. */
     dfb_input_detach_global( source, &connection->reaction );

     direct_list_remove( &device->connections, &connection->link );

     if (classes[device->clazz]->Disconnected)
          classes[device->clazz]->Disconnected( device, device->data, device->ctx, source );

     D_MAGIC_CLEAR( connection );

     SHFREE( context->shmpool, connection );
}

