/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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
#include <directfb_util.h>

#include <core/input.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <misc/conf.h>

#include <core/input_hub.h>

#define DFB_INPUTDRIVER_HAS_AXIS_INFO
#define DISABLE_INPUT_HOTPLUG_FUNCTION_STUB

#include <core/input_driver.h>

DFB_INPUT_DRIVER( input_hub )


D_DEBUG_DOMAIN( Input_Hub, "Input/Hub", "Input Hub driver" );


typedef struct {
     CoreInputDevice           *device;

     DFBInputDeviceID           device_id;
     DFBInputDeviceDescription  description;
} InputHubDeviceNode;


static CoreDFB                   *m_core;
static void                      *m_driver;
static CoreInputHubClient        *m_client;
static DirectHash                *m_nodes;


static void
InputHub_DeviceAdd( void                            *ctx,
                    DFBInputDeviceID                 device_id,
                    const DFBInputDeviceDescription *desc )
{
     DirectResult        ret;
     InputHubDeviceNode *node;

     D_DEBUG_AT( Input_Hub, "%s( ID %u, '%s' )\n", __FUNCTION__, device_id, desc->name );

     node = direct_hash_lookup( m_nodes, device_id );
     if (!node) {
          node = D_CALLOC( 1, sizeof(InputHubDeviceNode) );
          if (!node) {
               D_OOM();
               return;
          }

          node->device_id   = device_id;
          node->description = *desc;

          ret = direct_hash_insert( m_nodes, device_id, node );
          if (ret) {
               D_FREE( node );
               return;
          }

          ret = dfb_input_create_device( device_id, m_core, m_driver );
          if (ret) {
               direct_hash_remove( m_nodes, device_id );
               D_FREE( node );
               return;
          }
     }
     else
          D_WARN( "already have device (ID %u)", device_id );
}

static void
InputHub_DeviceRemove( void             *ctx,
                       DFBInputDeviceID  device_id )
{
     InputHubDeviceNode *node;

     D_DEBUG_AT( Input_Hub, "%s( ID %u )\n", __FUNCTION__, device_id );

     node = direct_hash_lookup( m_nodes, device_id );
     if (node) {
          dfb_input_remove_device( device_id, m_driver );

          direct_hash_remove( m_nodes, device_id );

          D_FREE( node );
     }
     else
          D_WARN( "don't have device (ID %u)", device_id );
}


static void
InputHub_EventDispatch( void                *ctx,
                        DFBInputDeviceID     device_id,
                        const DFBInputEvent *event )
{
     InputHubDeviceNode *node;

     D_DEBUG_AT( Input_Hub, "%s( ID %u, %s )\n", __FUNCTION__, device_id, dfb_input_event_type_name(event->type) );

     node = direct_hash_lookup( m_nodes, device_id );
     if (node) {
          if (node->device) {
               DFBInputEvent event_copy = *event;

               D_DEBUG_AT( Input_Hub, "  -> found device %p (ID %u)\n", node->device, dfb_input_device_id(node->device) );
          
               dfb_input_dispatch( node->device, &event_copy );
          }
          else
               D_WARN( "inactive device (ID %u)", device_id );
     }
     else
          D_WARN( "unknown device (ID %u)", device_id );
}


/* exported symbols */

/*
 * Return the number of available devices.
 * Called once during initialization of DirectFB.
 */
static int
driver_get_available( void )
{
     D_DEBUG_AT( Input_Hub, "%s()\n", __FUNCTION__ );

     return 0;
}

/*
 * Fill out general information about this driver.
 * Called once during initialization of DirectFB.
 */
static void
driver_get_info( InputDriverInfo *info )
{
     D_DEBUG_AT( Input_Hub, "%s()\n", __FUNCTION__ );

     /* fill driver info structure */
     snprintf ( info->name,
                DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "Input Hub Driver" );
     snprintf ( info->vendor,
                DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "directfb.org" );

     info->version.major = 1;
     info->version.minor = 0;
}



/*
 * Check if /dev/input/eventX is handled by the input device.  If so, return
 * DFB_OK.  Otherwise, return DFB_UNSUPPORTED.
 */
static DFBResult
is_created( int index, void *data )
{
     D_DEBUG_AT( Input_Hub, "%s( %d )\n", __FUNCTION__, index );

     D_ASSERT( data != NULL );

     if (index + 1 != (long) data)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

/*
 * Indicate that the hotplug detection capability is supported.
 */
static InputDriverCapability
get_capability( void )
{
     return IDC_HOTPLUG;
}

/*
 * Stop hotplug detection thread.
 */
static DFBResult
stop_hotplug( void )
{
     D_DEBUG_AT( Input_Hub, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

/*
 * Launch hotplug detection thread.
 */
static DFBResult
launch_hotplug( CoreDFB *core,
                void    *input_driver )
{
     DFBResult                   ret;
     CoreInputHubClientCallbacks callbacks;

     D_DEBUG_AT( Input_Hub, "%s()\n", __FUNCTION__ );

     if (!m_client && dfb_config->input_hub_qid) {
          D_ASSERT( m_nodes == NULL );

          ret = direct_hash_create( 17, &m_nodes );
          if (ret)
               return ret;

          D_DEBUG_AT( Input_Hub, "  -> creating input hub client...\n" );

          m_core   = core;
          m_driver = input_driver;

          memset( &callbacks, 0, sizeof(CoreInputHubClientCallbacks) );
     
          callbacks.DeviceAdd     = InputHub_DeviceAdd;
          callbacks.DeviceRemove  = InputHub_DeviceRemove;
          callbacks.EventDispatch = InputHub_EventDispatch;
     
          CoreInputHubClient_Create( dfb_config->input_hub_qid, &callbacks, NULL, &m_client );

          return CoreInputHubClient_Activate( m_client );
     }

     return DFB_OK;
}

/*
 * Enter the driver suspended state by setting the driver_suspended Boolean
 * to prevent hotplug events from being handled.
 */
static DFBResult
driver_suspend( void )
{
     return DFB_OK;
}

/*
 * Leave the driver suspended state by clearing the driver_suspended Boolean
 * which will allow hotplug events to be handled again.
 */
static DFBResult
driver_resume( void )
{
     return DFB_OK;
}


/*
 * Open the device, fill out information about it,
 * allocate and fill private data, start input thread.
 * Called during initialization, resuming or taking over mastership.
 */
static DFBResult
driver_open_device( CoreInputDevice  *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     InputHubDeviceNode *node;

     D_DEBUG_AT( Input_Hub, "%s( ID %u )\n", __FUNCTION__, number );

     node = direct_hash_lookup( m_nodes, number );
     if (!node) {
          D_BUG( "did not find device (ID %u)", number );
          return DFB_BUG;
     }

     info->prefered_id = number;
     info->desc        = node->description;

     node->device = device;

     *driver_data = (void*)(long) (number + 1);

     return DFB_OK;
}

/*
 * Fetch one entry from the kernel keymap.
 */
static DFBResult
driver_get_keymap_entry( CoreInputDevice           *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
     D_DEBUG_AT( Input_Hub, "%s( ID %u )\n", __FUNCTION__, dfb_input_device_id(device) );

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/*
 * Obtain information about an axis (only absolute axis so far).
 */
static DFBResult
driver_get_axis_info( CoreInputDevice              *device,
                      void                         *driver_data,
                      DFBInputDeviceAxisIdentifier  axis,
                      DFBInputDeviceAxisInfo       *ret_info )
{
     D_DEBUG_AT( Input_Hub, "%s( ID %u, axis %u )\n", __FUNCTION__, dfb_input_device_id(device), axis );

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/*
 * End thread, close device and free private data.
 */
static void
driver_close_device( void *driver_data )
{
     D_DEBUG_AT( Input_Hub, "%s()\n", __FUNCTION__ );
}

