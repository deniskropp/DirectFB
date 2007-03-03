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
#include <core/windows_internal.h>

#include <misc/util.h>

#include <unique/context.h>
#include <unique/device.h>
#include <unique/internal.h>


D_DEBUG_DOMAIN( UniQuE_Pointer, "UniQuE/Pointer", "UniQuE's Pointer Device Class" );


typedef struct {
     int magic;

     int x;
     int y;
} PointerData;

/**************************************************************************************************/

static DFBResult
pointer_initialize( UniqueDevice    *device,
                    void            *data,
                    void            *ctx )
{
     PointerData *pointer = data;

     D_MAGIC_ASSERT( device, UniqueDevice );

     D_DEBUG_AT( UniQuE_Pointer, "pointer_initialize( %p, %p, %p )\n", device, data, ctx );

     D_MAGIC_SET( pointer, PointerData );

     return DFB_OK;
}

static void
pointer_shutdown( UniqueDevice    *device,
                  void            *data,
                  void            *ctx )
{
     PointerData *pointer = data;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( pointer, PointerData );

     D_DEBUG_AT( UniQuE_Pointer, "pointer_shutdown( %p, %p, %p )\n", device, data, ctx );

     D_MAGIC_CLEAR( pointer );
}

static void
pointer_connected( UniqueDevice    *device,
                   void            *data,
                   void            *ctx,
                   CoreInputDevice *source )
{
     PointerData *pointer = data;

     (void) pointer;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( pointer, PointerData );

     D_ASSERT( source != NULL );

     D_DEBUG_AT( UniQuE_Pointer, "pointer_connected( %p, %p, %p, %p )\n",
                 device, data, ctx, source );
}

static void
pointer_disconnected( UniqueDevice    *device,
                      void            *data,
                      void            *ctx,
                      CoreInputDevice *source )
{
     PointerData *pointer = data;

     (void) pointer;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( pointer, PointerData );

     D_ASSERT( source != NULL );

     D_DEBUG_AT( UniQuE_Pointer, "pointer_disconnected( %p, %p, %p, %p )\n",
                 device, data, ctx, source );
}

static void
pointer_process_event( UniqueDevice        *device,
                       void                *data,
                       void                *ctx,
                       const DFBInputEvent *event )
{
     UniqueInputEvent  evt;
     PointerData      *pointer = data;
     UniqueContext    *context = ctx;
     CoreWindowStack  *stack;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( pointer, PointerData );
     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( event != NULL );

     D_DEBUG_AT( UniQuE_Pointer, "pointer_process_event( %p, %p, %p, %p ) <- type 0x%08x\n",
                 device, data, ctx, event, event->type );

     stack = context->stack;

     D_ASSERT( stack != NULL );

     switch (event->type) {
          case DIET_AXISMOTION: {
               /*int x = pointer->x;
               int y = pointer->y;*/
               int x = stack->cursor.x;
               int y = stack->cursor.y;

               if (event->flags & DIEF_AXISREL) {
                    int rel = event->axisrel;

                    /* handle cursor acceleration */
                    if (rel > stack->cursor.threshold)
                         rel += (rel - stack->cursor.threshold)
                                * stack->cursor.numerator
                                / stack->cursor.denominator;
                    else if (rel < -stack->cursor.threshold)
                         rel += (rel + stack->cursor.threshold)
                                * stack->cursor.numerator
                                / stack->cursor.denominator;

                    switch (event->axis) {
                         case DIAI_X:
                              x += rel;
                              break;

                         case DIAI_Y:
                              y += rel;
                              break;

                         default:
                              return;
                    }
               }
               else if (event->flags & DIEF_AXISABS) {
                    switch (event->axis) {
                         case DIAI_X:
                              x = event->axisabs;
                              break;

                         case DIAI_Y:
                              y = event->axisabs;
                              break;

                         default:
                              return;
                    }
               }
               else
                    return;

               if (x < 0)
                    x = 0;
               else if (x >= context->width)
                    x = context->width - 1;

               if (y < 0)
                    y = 0;
               else if (y >= context->height)
                    y = context->height - 1;

               if (x == pointer->x && y == pointer->y)
                    return;

               pointer->x = x;
               pointer->y = y;

               evt.type = UIET_MOTION;

               evt.pointer.device_id = event->device_id;
               evt.pointer.x         = x;
               evt.pointer.y         = y;
               evt.pointer.buttons   = event->buttons;

               unique_device_dispatch( device, &evt );
               break;
          }

          case DIET_BUTTONPRESS:
          case DIET_BUTTONRELEASE:
               evt.type = UIET_BUTTON;

               evt.pointer.device_id = event->device_id;
               evt.pointer.press     = (event->type == DIET_BUTTONPRESS);
               evt.pointer.x         = pointer->x;
               evt.pointer.y         = pointer->y;
               evt.pointer.button    = event->button;
               evt.pointer.buttons   = event->buttons;

               unique_device_dispatch( device, &evt );
               break;

          default:
               break;
     }
}


const UniqueDeviceClass unique_pointer_device_class = {
     data_size:     sizeof(PointerData),

     Initialize:    pointer_initialize,
     Shutdown:      pointer_shutdown,
     Connected:     pointer_connected,
     Disconnected:  pointer_disconnected,
     ProcessEvent:  pointer_process_event
};

