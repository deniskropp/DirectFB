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
#include <core/windowstack.h>

#include <misc/util.h>

#include <unique/device.h>
#include <unique/input_channel.h>
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

static void purge_connection( UniqueInputSwitch      *input_switch,
                              SwitchConnection       *connection );

static bool target_switch   ( UniqueInputSwitch      *input_switch,
                              UniqueDeviceClassIndex  index,
                              UniqueInputChannel     *channel );

static bool update_targets  ( UniqueInputSwitch      *input_switch );

/**************************************************************************************************/

DFBResult
unique_input_switch_create( UniqueContext      *context,
                            UniqueInputSwitch **ret_switch )
{
     int                i;
     WMShared          *shared;
     UniqueInputSwitch *input_switch;

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_create( context %p )\n", context );

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( ret_switch != NULL );

     shared = context->shared;

     D_MAGIC_ASSERT( shared, WMShared );

     /* Allocate switch data. */
     input_switch = SHCALLOC( context->shmpool, 1, sizeof(UniqueInputSwitch) );
     if (!input_switch) {
          D_WARN( "out of (shared) memory" );
          return D_OOSHM();
     }

     /* Initialize input_switch data. */
     input_switch->context = context;

     /* Set class ID of each target. */
     for (i=0; i<_UDCI_NUM; i++)
          input_switch->targets[i].clazz = shared->device_classes[i];

     D_MAGIC_SET( input_switch, UniqueInputSwitch );

     D_DEBUG_AT( UniQuE_InpSw, "  -> input_switch created (%p)\n", input_switch );

     *ret_switch = input_switch;

     return DFB_OK;
}

DFBResult
unique_input_switch_destroy( UniqueInputSwitch *input_switch )
{
     int               i;
     DirectLink       *n;
     SwitchConnection *connection;
     UniqueContext    *context;

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );

     context = input_switch->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_destroy( %p )\n", input_switch );

     direct_list_foreach_safe (connection, n, input_switch->connections) {
          D_MAGIC_ASSERT( connection, SwitchConnection );

          purge_connection( input_switch, connection );
     }

     D_ASSERT( input_switch->connections == NULL );

     for (i=0; i<_UDCI_NUM; i++) {
          UniqueInputFilter *filter;
          UniqueInputTarget *target = &input_switch->targets[i];

          direct_list_foreach_safe (filter, n, target->filters) {
               D_MAGIC_ASSERT( filter, UniqueInputFilter );
               D_MAGIC_ASSERT( filter->channel, UniqueInputChannel );

               D_DEBUG_AT( UniQuE_InpSw, "  -> filter %p, index %d, channel %p\n",
                           filter, filter->index, filter->channel );

               direct_list_remove( &target->filters, &filter->link );

               D_MAGIC_CLEAR( filter );

               SHFREE( context->shmpool, filter );
          }

          D_ASSERT( target->filters == NULL );
     }

     D_MAGIC_CLEAR( input_switch );

     SHFREE( context->shmpool, input_switch );

     return DFB_OK;
}

DFBResult
unique_input_switch_add( UniqueInputSwitch *input_switch,
                         UniqueDevice      *device )
{
     DFBResult         ret;
     SwitchConnection *connection;
     UniqueContext    *context;

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( device, UniqueDevice );

     context = input_switch->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_add( %p, %p )\n", input_switch, device );

     /* Allocate connection structure. */
     connection = SHCALLOC( context->shmpool, 1, sizeof(SwitchConnection) );
     if (!connection) {
          D_WARN( "out of (shared) memory" );
          return D_OOSHM();
     }

     /* Initialize connection structure. */
     connection->device = device;

     /* Attach global reaction for processing events. */
     ret = unique_device_attach_global( device, UNIQUE_INPUT_SWITCH_DEVICE_LISTENER,
                                        input_switch, &connection->reaction );
     if (ret) {
          D_DERROR( ret, "UniQuE/InpSwitch: Could not attach global device reaction!\n" );
          SHFREE( context->shmpool, connection );
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
     UniqueInputTarget *target;

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_select( %p, %d, %p )\n",
                 input_switch, index, channel );

     D_ASSERT( index >= 0 );
     D_ASSERT( index < _UDCI_NUM );

     target = &input_switch->targets[index];

     target->normal = channel;

     if (!target->fixed && !target->implicit)
          target_switch( input_switch, index, channel );

     return DFB_OK;
}

DFBResult
unique_input_switch_set( UniqueInputSwitch      *input_switch,
                         UniqueDeviceClassIndex  index,
                         UniqueInputChannel     *channel )
{
     UniqueInputTarget *target;

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_set( %p, %d, %p )\n",
                 input_switch, index, channel );

     D_ASSERT( index >= 0 );
     D_ASSERT( index < _UDCI_NUM );

     target = &input_switch->targets[index];

     if (target->fixed)
          return DFB_BUSY;

     target->fixed = channel;

     target_switch( input_switch, index, channel );

     return DFB_OK;
}

DFBResult
unique_input_switch_unset( UniqueInputSwitch      *input_switch,
                           UniqueDeviceClassIndex  index,
                           UniqueInputChannel     *channel )
{
     UniqueInputTarget *target;

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_unset( %p, %d, %p )\n",
                 input_switch, index, channel );

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     D_ASSERT( index >= 0 );
     D_ASSERT( index < _UDCI_NUM );

     target = &input_switch->targets[index];

     if (target->fixed != channel)
          return DFB_ACCESSDENIED;

     target->fixed = NULL;


     update_targets( input_switch );
     //target_switch( input_switch, index, target->normal );

     return DFB_OK;
}

DFBResult
unique_input_switch_set_filter( UniqueInputSwitch       *input_switch,
                                UniqueDeviceClassIndex   index,
                                UniqueInputChannel      *channel,
                                const UniqueInputEvent  *event,
                                UniqueInputFilter      **ret_filter )
{
     UniqueInputFilter *filter;
     UniqueInputTarget *target;
     UniqueContext     *context;

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_set_filter( %p, %d, %p, %p )\n",
                 input_switch, index, channel, event );

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     D_ASSERT( index >= 0 );
     D_ASSERT( index < _UDCI_NUM );

     D_ASSERT( event != NULL );
     D_ASSERT( ret_filter != NULL );

     context = input_switch->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     target = &input_switch->targets[index];

     direct_list_foreach (filter, target->filters) {
          D_MAGIC_ASSERT( filter, UniqueInputFilter );

          if (unique_device_filter( target->clazz, event, &filter->filter ))
               return DFB_BUSY;
     }

     /* Allocate new filter. */
     filter = SHCALLOC( context->shmpool, 1, sizeof(UniqueInputFilter) );
     if (!filter)
          return D_OOSHM();

     filter->index   = index;
     filter->channel = channel;
     filter->filter  = *event;

     direct_list_append( &target->filters, &filter->link );

     D_MAGIC_SET( filter, UniqueInputFilter );

     *ret_filter = filter;

     return DFB_OK;
}

DFBResult
unique_input_switch_unset_filter( UniqueInputSwitch *input_switch,
                                  UniqueInputFilter *filter )
{
     UniqueInputTarget *target;
     UniqueContext     *context;

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_unset_filter( %p, %p )\n",
                 input_switch, filter );

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( filter, UniqueInputFilter );

     context = input_switch->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( filter->index >= 0 );
     D_ASSERT( filter->index < _UDCI_NUM );

     target = &input_switch->targets[filter->index];

     direct_list_remove( &target->filters, &filter->link );

     D_MAGIC_CLEAR( filter );

     SHFREE( context->shmpool, filter );

     return DFB_OK;
}

DFBResult
unique_input_switch_drop( UniqueInputSwitch  *input_switch,
                          UniqueInputChannel *channel )
{
     int            i;
     UniqueContext *context;

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_drop( %p, %p )\n", input_switch, channel );

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     context = input_switch->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     for (i=0; i<_UDCI_NUM; i++) {
          DirectLink        *n;
          UniqueInputFilter *filter;
          UniqueInputTarget *target = &input_switch->targets[i];

          if (target->normal == channel)
               target->normal = NULL;

          if (target->fixed == channel)
               target->fixed = NULL;

          if (target->implicit == channel)
               target->implicit = NULL;

          if (target->current == channel)
               target->current = NULL;

          D_DEBUG_AT( UniQuE_InpSw, "  -> index %d, filters %p\n", i, target->filters );

          direct_list_foreach_safe (filter, n, target->filters) {
               D_MAGIC_ASSERT( filter, UniqueInputFilter );
               D_MAGIC_ASSERT( filter->channel, UniqueInputChannel );

               D_DEBUG_AT( UniQuE_InpSw,
                           "  -> filter %p, channel %p\n", filter, filter->channel );

               D_ASSUME( filter->channel != channel );

               if (filter->channel == channel) {
                    direct_list_remove( &target->filters, &filter->link );

                    D_MAGIC_CLEAR( filter );

                    SHFREE( context->shmpool, filter );
               }
          }
     }

     if (!input_switch->targets[UDCI_POINTER].fixed)
          update_targets( input_switch );

     return DFB_OK;
}

DFBResult
unique_input_switch_update( UniqueInputSwitch  *input_switch,
                            UniqueInputChannel *channel )
{
     int            x, y, i;
     StretRegion   *region;
     UniqueContext *context;

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );

     x = input_switch->x;
     y = input_switch->y;

     D_DEBUG_AT( UniQuE_InpSw, "unique_input_switch_update( %d, %d )\n", x, y );

     context = input_switch->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     region = stret_region_at( context->root, x, y, SRF_INPUT, SRCID_UNKNOWN );
     if (region) {
          for (i=0; i<_UDCI_NUM; i++) {
               UniqueInputTarget *target = &input_switch->targets[i];

               if (target->normal == channel)
                    stret_region_get_input( region, i, x, y, &target->normal );
          }
     }
     else {
          for (i=0; i<_UDCI_NUM; i++) {
               UniqueInputTarget *target = &input_switch->targets[i];

               if (target->normal == channel)
                    target->normal = NULL;
          }
     }

     for (i=0; i<_UDCI_NUM; i++) {
          UniqueInputTarget *target = &input_switch->targets[i];

          target_switch( input_switch, i, target->fixed ? : target->implicit ? : target->normal );
     }

     return DFB_OK;
}

/**************************************************************************************************/

static bool
target_switch( UniqueInputSwitch      *input_switch,
               UniqueDeviceClassIndex  index,
               UniqueInputChannel     *channel )
{
     UniqueInputEvent    evt;
     UniqueInputTarget  *target;
     UniqueInputChannel *current;

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_ASSERT( index >= 0 );
     D_ASSERT( index < _UDCI_NUM );

     target = &input_switch->targets[index];

     current = target->current;

     D_MAGIC_ASSERT_IF( channel, UniqueInputChannel );
     D_MAGIC_ASSERT_IF( current, UniqueInputChannel );

     if (channel == current)
          return false;

     D_DEBUG_AT( UniQuE_InpSw, "target_switch( index %d, x %d, y %d, channel %p )\n",
                 index, input_switch->x, input_switch->y, channel );

     evt.type = UIET_CHANNEL;

     evt.channel.index = index;
     evt.channel.x     = input_switch->x;
     evt.channel.y     = input_switch->y;

     if (current) {
          evt.channel.selected = false;

          unique_input_channel_dispatch( current, &evt );
     }

     target->current = channel;

     if (channel) {
          evt.channel.selected = true;

          unique_input_channel_dispatch( channel, &evt );
     }

     return true;
}

static void
target_dispatch( UniqueInputTarget      *target,
                 const UniqueInputEvent *event )
{
     UniqueInputFilter  *filter;
     UniqueInputChannel *channel;

     D_ASSERT( target != NULL );
     D_ASSERT( event != NULL );

     channel = target->current;

     D_MAGIC_ASSERT_IF( channel, UniqueInputChannel );


     direct_list_foreach (filter, target->filters) {
          D_MAGIC_ASSERT( filter, UniqueInputFilter );

          if (unique_device_filter( target->clazz, event, &filter->filter )) {
               channel = filter->channel;

               D_MAGIC_ASSERT( channel, UniqueInputChannel );

               break;
          }
     }

     if (channel)
          unique_input_channel_dispatch( channel, event );
     else
          D_DEBUG_AT( UniQuE_InpSw, "target_dispatch( class %d ) "
                      "<- no selected channel, dropping event.\n", target->clazz );
}

static bool
update_targets( UniqueInputSwitch *input_switch )
{
     int            x, y, i;
     StretRegion   *region;
     UniqueContext *context;
     bool           updated[_UDCI_NUM];

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );

     x = input_switch->x;
     y = input_switch->y;

     D_DEBUG_AT( UniQuE_InpSw, "update_targets( %d, %d )\n", x, y );

     context = input_switch->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     region = stret_region_at( context->root, x, y, SRF_INPUT, SRCID_UNKNOWN );
     if (region) {
          for (i=0; i<_UDCI_NUM; i++)
               stret_region_get_input( region, i, x, y, &input_switch->targets[i].normal );
     }
     else {
          for (i=0; i<_UDCI_NUM; i++)
               input_switch->targets[i].normal = NULL;
     }

     for (i=0; i<_UDCI_NUM; i++) {
          UniqueInputTarget *target = &input_switch->targets[i];

          updated[i] = target_switch( input_switch, i,
                                      target->fixed ? : target->implicit ? : target->normal );
     }

     return updated[UDCI_POINTER];
}

ReactionResult
_unique_input_switch_device_listener( const void *msg_data,
                                      void       *ctx )
{
     const UniqueInputEvent *event  = msg_data;
     UniqueInputSwitch      *inpsw  = ctx;
     UniqueInputTarget      *target = NULL;
     UniqueContext          *context;

     (void) event;
     (void) inpsw;

     D_ASSERT( event != NULL );

     D_MAGIC_ASSERT( inpsw, UniqueInputSwitch );

     context = inpsw->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     if (dfb_windowstack_lock( context->stack ))
          return RS_OK;

     if (!context->active) {
          dfb_windowstack_unlock( context->stack );
          return RS_OK;
     }

     D_DEBUG_AT( UniQuE_InpSw, "_unique_input_switch_device_listener( %p, %p )\n",
                 event, inpsw );

     switch (event->type) {
          case UIET_MOTION:
               target = &inpsw->targets[UDCI_POINTER];

               inpsw->x = event->pointer.x;
               inpsw->y = event->pointer.y;

               if (!target->fixed && !target->implicit && update_targets( inpsw ))
                    break;

               target_dispatch( target, event );
               break;

          case UIET_BUTTON:
               target = &inpsw->targets[UDCI_POINTER];

               if (event->pointer.press && !target->implicit) {
                    D_DEBUG_AT( UniQuE_InpSw, "  -> implicit pointer grab, %p\n", target->current );

                    target->implicit = target->current;
               }

               target_dispatch( target, event );

               if (!event->pointer.press && !event->pointer.buttons) {
                    D_ASSUME( target->implicit != NULL );

                    if (target->implicit) {
                         D_DEBUG_AT( UniQuE_InpSw, "  -> implicit pointer ungrab, %p\n", target->implicit );

                         target->implicit = NULL;

                         if (!target->fixed)
                              update_targets( inpsw );
                    }
               }
               break;

          case UIET_WHEEL:
               target_dispatch( &inpsw->targets[UDCI_WHEEL], event );
               break;

          case UIET_KEY:
               target = &inpsw->targets[UDCI_KEYBOARD];

               if (event->keyboard.press && !target->implicit) {
                    D_DEBUG_AT( UniQuE_InpSw, "  -> implicit keyboard grab, %p\n", target->current );

                    target->implicit = target->current;
               }

               target_dispatch( target, event );

               if (!event->keyboard.press && !event->keyboard.modifiers) {
                    //D_ASSUME( target->implicit != NULL );

                    if (target->implicit) {
                         D_DEBUG_AT( UniQuE_InpSw, "  -> implicit keyboard ungrab, %p\n", target->implicit );

                         if (!target->fixed)
                              target_switch( inpsw, UDCI_KEYBOARD, target->normal );

                         target->implicit = NULL;
                    }
               }
               break;

          default:
               D_ONCE( "unknown event type" );
               break;
     }

     dfb_windowstack_unlock( context->stack );

     return RS_OK;
}

/**************************************************************************************************/

static void
purge_connection( UniqueInputSwitch *input_switch,
                  SwitchConnection  *connection )
{
     UniqueContext *context;

     D_DEBUG_AT( UniQuE_InpSw, "purge_connection( %p, %p )\n", input_switch, connection );

     D_MAGIC_ASSERT( input_switch, UniqueInputSwitch );
     D_MAGIC_ASSERT( connection, SwitchConnection );

     context = input_switch->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     /* Detach global reaction for receiving events. */
     unique_device_detach_global( connection->device, &connection->reaction );

     direct_list_remove( &input_switch->connections, &connection->link );

     D_MAGIC_CLEAR( connection );

     SHFREE( context->shmpool, connection );
}

