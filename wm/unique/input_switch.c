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

#include <core/input.h>

#include <misc/util.h>

#include <unique/device.h>
#include <unique/input_switch.h>
#include <unique/internal.h>



D_DEBUG_DOMAIN( UniQuE_InpSw, "UniQuE/InpSwitch", "UniQuE's Input Switch" );


typedef struct {
     DirectLink       link;

     int              magic;

     UniqueDevice    *device;

     GlobalReaction   reaction;
} SwitchConnection;

/**************************************************************************************************/

static void purge_connection( UniqueInputSwitch *input_switch,
                              SwitchConnection  *connection );

/**************************************************************************************************/

DFBResult
unique_input_switch_create( UniqueContext      *context,
                            UniqueInputSwitch **ret_switch )
{
     UniqueInputSwitch *input_switch;

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_create( context %p )\n", context );

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( ret_switch != NULL );

     /* Allocate switch data. */
     input_switch = SHCALLOC( 1, sizeof(UniqueInputSwitch) );
     if (!input_switch) {
          D_WARN( "out of (shared) memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     /* Initialize input_switch data. */
     input_switch->context = context;

     D_MAGIC_SET( input_switch, UniqueInputSwitch );

     D_DEBUG_AT( UniQuE_InpSw, "  -> input_switch created (%p)\n", input_switch );

     *ret_switch = input_switch;

     return DFB_OK;
}

DFBResult
unique_input_switch_destroy( UniqueInputSwitch *input_switch )
{
     DirectLink       *n;
     SwitchConnection *connection;

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_destroy( %p )\n", input_switch );

     direct_list_foreach_safe (connection, n, input_switch->connections) {
          D_MAGIC_ASSERT( connection, SwitchConnection );

          purge_connection( input_switch, connection );
     }

     D_ASSERT( input_switch->connections == NULL );

     D_MAGIC_CLEAR( input_switch );

     SHFREE( input_switch );

     return DFB_OK;
}

DFBResult
unique_input_switch_add( UniqueInputSwitch *input_switch,
                         UniqueDevice      *device )
{
     DFBResult         ret;
     SwitchConnection *connection;

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( device, UniqueDevice );

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_add( %p, %p )\n", input_switch, device );

     /* Allocate connection structure. */
     connection = SHCALLOC( 1, sizeof(SwitchConnection) );
     if (!connection) {
          D_WARN( "out of (shared) memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     /* Initialize connection structure. */
     connection->device = device;

     /* Attach global reaction for processing events. */
     ret = unique_device_attach_global( device, UNIQUE_INPUT_SWITCH_DEVICE_LISTENER,
                                        input_switch, &connection->reaction );
     if (ret) {
          D_DERROR( ret, "UniQuE/InpSwitch: Could not attach global device reaction!\n" );
          SHFREE( connection );
          return ret;
     }

     /* Add the new connection to the list. */
     direct_list_append( &input_switch->connections, &connection->link );

     D_MAGIC_SET( connection, SwitchConnection );

     return DFB_OK;
}

DFBResult
unique_input_switch_remove( UniqueInputSwitch *input_switch,
                            UniqueDevice      *device )
{
     SwitchConnection *connection;

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( device, UniqueDevice );

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_remove( %p, %p )\n", input_switch, device );

     direct_list_foreach (connection, input_switch->connections) {
          D_MAGIC_ASSERT( connection, SwitchConnection );

          if (connection->device == device)
               break;
     }

     if (!connection) {
          D_WARN( "device not found amoung connections" );
          return DFB_ITEMNOTFOUND;
     }

     purge_connection( input_switch, connection );

     return DFB_OK;
}

DFBResult
unique_input_switch_select( UniqueInputSwitch      *input_switch,
                            UniqueDeviceClassIndex  index,
                            UniqueInputChannel     *channel )
{
     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
//     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_select( %p, %d, %p )\n",
                 input_switch, index, channel );

     return DFB_OK;
}

/**************************************************************************************************/

ReactionResult
_unique_input_switch_device_listener( const void *msg_data,
                                      void       *ctx )
{
     const UniqueInputEvent *event = msg_data;
     UniqueInputSwitch      *inpsw = ctx;

     D_ASSERT( event != NULL );

     D_MAGIC_ASSERT( inpsw, UniqueInputSwitch );

     D_DEBUG_AT( UniQuE_InpSw, "_unique_input_switch_device_listener( %p, %p )\n",
                 event, inpsw );

     return RS_OK;
}

/**************************************************************************************************/

static void
purge_connection( UniqueInputSwitch *input_switch,
                  SwitchConnection  *connection )
{
     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( connection, SwitchConnection );

     D_DEBUG_AT( UniQuE_InpSw, "  -> purge_connection( %p, %p )\n", input_switch, connection );

     /* Detach global reaction for receiving events. */
     unique_device_detach_global( connection->device, &connection->reaction );

     direct_list_remove( &input_switch->connections, &connection->link );

     D_MAGIC_CLEAR( connection );

     SHFREE( connection );
}

