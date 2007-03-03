/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/gfxcard.h>
#include <core/state.h>

#include <misc/util.h>

#include <unique/context.h>
#include <unique/device.h>
#include <unique/internal.h>


D_DEBUG_DOMAIN( UniQuE_Keyboard, "UniQuE/Keyboard", "UniQuE's Keyboard Device Class" );


typedef struct {
     int magic;
} KeyboardData;

/**************************************************************************************************/

static DFBResult
keyboard_initialize( UniqueDevice    *device,
                     void            *data,
                     void            *ctx )
{
     KeyboardData *keyboard = data;

     D_MAGIC_ASSERT( device, UniqueDevice );

     D_DEBUG_AT( UniQuE_Keyboard, "keyboard_initialize( %p, %p, %p )\n", device, data, ctx );

     D_MAGIC_SET( keyboard, KeyboardData );

     return DFB_OK;
}

static void
keyboard_shutdown( UniqueDevice    *device,
                   void            *data,
                   void            *ctx )
{
     KeyboardData *keyboard = data;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( keyboard, KeyboardData );

     D_DEBUG_AT( UniQuE_Keyboard, "keyboard_shutdown( %p, %p, %p )\n", device, data, ctx );

     D_MAGIC_CLEAR( keyboard );
}

static void
keyboard_connected( UniqueDevice        *device,
                    void                *data,
                    void                *ctx,
                    CoreInputDevice     *source )
{
     KeyboardData *keyboard = data;

     (void) keyboard;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( keyboard, KeyboardData );

     D_ASSERT( source != NULL );

     D_DEBUG_AT( UniQuE_Keyboard, "keyboard_connected( %p, %p, %p, %p )\n",
                 device, data, ctx, source );
}

static void
keyboard_disconnected( UniqueDevice        *device,
                       void                *data,
                       void                *ctx,
                       CoreInputDevice     *source )
{
     KeyboardData *keyboard = data;

     (void) keyboard;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( keyboard, KeyboardData );

     D_ASSERT( source != NULL );

     D_DEBUG_AT( UniQuE_Keyboard, "keyboard_disconnected( %p, %p, %p, %p )\n",
                 device, data, ctx, source );
}

static void
keyboard_process_event( UniqueDevice        *device,
                        void                *data,
                        void                *ctx,
                        const DFBInputEvent *event )
{
     UniqueInputEvent  evt;
     KeyboardData     *keyboard = data;

     (void) keyboard;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( keyboard, KeyboardData );

     D_ASSERT( event != NULL );

     D_DEBUG_AT( UniQuE_Keyboard, "keyboard_process_event( %p, %p, %p, %p ) <- type 0x%08x\n",
                 device, data, ctx, event, event->type );

     switch (event->type) {
          case DIET_KEYPRESS:
          case DIET_KEYRELEASE:
               evt.type = UIET_KEY;

               evt.keyboard.device_id  = event->device_id;
               evt.keyboard.press      = (event->type == DIET_KEYPRESS);
               evt.keyboard.key_code   = event->key_code;
               evt.keyboard.key_id     = event->key_id;
               evt.keyboard.key_symbol = event->key_symbol;
               evt.keyboard.modifiers  = event->modifiers;
               evt.keyboard.locks      = event->locks;

               unique_device_dispatch( device, &evt );
               break;

          default:
               break;
     }
}

static bool
keyboard_filter_event( const UniqueInputEvent *event,
                       const UniqueInputEvent *filter )
{
     D_ASSERT( event != NULL );
     D_ASSERT( filter != NULL );

     D_DEBUG_AT( UniQuE_Keyboard, "keyboard_filter_event( %p, %p )\n", event, filter );

     if (filter->keyboard.key_code != -1)
          return (filter->keyboard.key_code == event->keyboard.key_code);

     return (event->keyboard.modifiers  == filter->keyboard.modifiers &&
             event->keyboard.key_symbol == filter->keyboard.key_symbol);
}


const UniqueDeviceClass unique_keyboard_device_class = {
     data_size:     sizeof(KeyboardData),

     Initialize:    keyboard_initialize,
     Shutdown:      keyboard_shutdown,
     Connected:     keyboard_connected,
     Disconnected:  keyboard_disconnected,
     ProcessEvent:  keyboard_process_event,
     FilterEvent:   keyboard_filter_event
};

