/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sched.h>

#include <malloc.h>
#include <sys/time.h>
#include <errno.h>

#include <pthread.h>

#include "directfb.h"
#include "directfb_internals.h"

#include "core/core.h"
#include "core/coredefs.h"
#include "core/coretypes.h"

#include "core/state.h"
#include "core/surfaces.h"
#include "core/windows.h"

#include "display/idirectfbsurface.h"
#include "display/idirectfbsurface_window.h"

#include "input/idirectfbinputbuffer.h"

#include "misc/util.h"
#include "misc/mem.h"

#include "gfx/convert.h"

#include "idirectfbwindow.h"

/*
 * adds an window event to the event queue
 */
static ReactionResult IDirectFBWindow_React( const void *msg_data,
                                             void       *ctx );



typedef struct {
     int                ref;
     CoreWindow        *window;

     IDirectFBSurface  *surface;

     DFBWindowEvent    *position_size_event;
} IDirectFBWindow_data;


static void
IDirectFBWindow_Destruct( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (data->surface)
          data->surface->Release( data->surface );

     if (data->window) {
          dfb_window_detach( data->window, IDirectFBWindow_React, data );
          
          /* recheck, threading... */
          if (data->window) {
               dfb_window_deinit( data->window );
               dfb_window_destroy( data->window, true );
          }
     }

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBWindow_AddRef( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Release( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (--data->ref == 0)
          IDirectFBWindow_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_CreateEventBuffer( IDirectFBWindow       *thiz,
                                   IDirectFBEventBuffer **buffer )
{
     IDirectFBEventBuffer *b;

     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     DFB_ALLOCATE_INTERFACE( b, IDirectFBEventBuffer );

     IDirectFBEventBuffer_Construct( b );

     IDirectFBEventBuffer_AttachWindow( b, data->window );

     if (data->position_size_event) {
          dfb_window_dispatch( data->window, data->position_size_event );
          DFBFREE( data->position_size_event );
          data->position_size_event = NULL;
     }

     *buffer = b;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_AttachEventBuffer( IDirectFBWindow       *thiz,
                                   IDirectFBEventBuffer  *buffer )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     IDirectFBEventBuffer_AttachWindow( buffer, data->window );

     if (data->position_size_event) {
          dfb_window_dispatch( data->window, data->position_size_event );
          DFBFREE( data->position_size_event );
          data->position_size_event = NULL;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_EnableEvents( IDirectFBWindow       *thiz,
                              DFBWindowEventType     mask )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (mask & ~DWET_ALL)
          return DFB_INVARG;

     data->window->events |= mask;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_DisableEvents( IDirectFBWindow       *thiz,
                               DFBWindowEventType     mask )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (mask & ~DWET_ALL)
          return DFB_INVARG;

     data->window->events &= ~mask;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_GetID( IDirectFBWindow *thiz,
                       DFBWindowID     *id )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (!id)
          return DFB_INVARG;

     *id = data->window->id;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_GetPosition( IDirectFBWindow *thiz,
                             int             *x,
                             int             *y )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (!x && !y)
          return DFB_INVARG;

     if (x)
          *x = data->window->x;

     if (y)
          *y = data->window->y;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_GetSize( IDirectFBWindow *thiz,
                         unsigned int    *width,
                         unsigned int    *height )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (!width && !height)
          return DFB_INVARG;

     if (width)
          *width = data->window->width;

     if (height)
          *height = data->window->height;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_GetSurface( IDirectFBWindow   *thiz,
                            IDirectFBSurface **surface )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (!surface)
          return DFB_INVARG;

     if (data->window->caps & DWCAPS_INPUTONLY)
          return DFB_UNSUPPORTED;

     if (!data->surface) {
          DFBResult ret;

          DFB_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

          ret = IDirectFBSurface_Window_Construct( *surface,
                                                   NULL, NULL, data->window,
                                                   DSCAPS_FLIPPING );
          if (ret)
               return ret;

          data->surface = *surface;
     }
     else
          *surface = data->surface;

     data->surface->AddRef( data->surface );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_SetOptions( IDirectFBWindow  *thiz,
                            DFBWindowOptions  options )
{
     DFBWindowOptions changed;

     INTERFACE_GET_DATA(IDirectFBWindow)

     /* Check arguments */
     if (!data->window)
          return DFB_DESTROYED;

     if (options & ~DWOP_ALL)
          return DFB_INVARG;

     if ((options & DWOP_ALPHACHANNEL) &&
         !(data->window->caps & DWCAPS_ALPHACHANNEL))
          return DFB_UNSUPPORTED;

     /* Examine toggled options */
     changed = data->window->options ^ options;

     /* Set new options */
     data->window->options = options;

     /* Redraw window if appearance influencing options have been toggled */
     if (changed & (DWOP_COLORKEYING | DWOP_ALPHACHANNEL))
          dfb_window_repaint( data->window, NULL, 0 );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_GetOptions( IDirectFBWindow  *thiz,
                            DFBWindowOptions *options )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (!options)
          return DFB_INVARG;

     *options = data->window->options;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_SetColorKey( IDirectFBWindow *thiz,
                             __u8             r,
                             __u8             g,
                             __u8             b )
{
     __u32 key;

     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (data->window->caps & DWCAPS_INPUTONLY)
          return DFB_UNSUPPORTED;

     key = color_to_pixel( data->window->surface->format, r, g, b );

     if (data->window->color_key != key) {
          data->window->color_key = key;

          if (data->window->options & DWOP_COLORKEYING)
               dfb_window_repaint( data->window, NULL, 0 );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_SetOpacity( IDirectFBWindow *thiz,
                            __u8             opacity )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (data->window->opacity != opacity)
          dfb_window_set_opacity( data->window, opacity );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_GetOpacity( IDirectFBWindow *thiz,
                            __u8            *opacity )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (!opacity)
          return DFB_INVARG;

     *opacity = data->window->opacity;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_RequestFocus( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     dfb_window_request_focus( data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_GrabKeyboard( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     return dfb_window_grab_keyboard( data->window );
}

static DFBResult
IDirectFBWindow_UngrabKeyboard( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     return dfb_window_ungrab_keyboard( data->window );
}

static DFBResult
IDirectFBWindow_GrabPointer( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     return dfb_window_grab_pointer( data->window );
}

static DFBResult
IDirectFBWindow_UngrabPointer( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     return dfb_window_ungrab_pointer( data->window );
}

static DFBResult
IDirectFBWindow_Move( IDirectFBWindow *thiz, int dx, int dy )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (dx == 0  &&  dy == 0)
          return DFB_OK;

     dfb_window_move( data->window, dx, dy );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_MoveTo( IDirectFBWindow *thiz, int x, int y )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (data->window->x == x  &&  data->window->y == y)
          return DFB_OK;

     dfb_window_move( data->window,
                      x - data->window->x, y - data->window->y );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Resize( IDirectFBWindow *thiz,
                        unsigned int     width,
                        unsigned int     height )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (!width || width > 4096 || !height || height > 4096)
          return DFB_INVARG;
     
     if (data->window->width == width  &&  data->window->height == height)
          return DFB_OK;

     return dfb_window_resize( data->window, width, height );
}

static DFBResult
IDirectFBWindow_Raise( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     dfb_window_raise( data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_SetStackingClass( IDirectFBWindow        *thiz,
                                  DFBWindowStackingClass  stacking_class )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     switch (stacking_class) {
          case DWSC_MIDDLE:
          case DWSC_UPPER:
          case DWSC_LOWER:
               break;
          default:
               return DFB_INVARG;
     }

     dfb_window_change_stacking( data->window, stacking_class );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Lower( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     dfb_window_lower( data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_RaiseToTop( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     dfb_window_raisetotop( data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_LowerToBottom( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     dfb_window_lowertobottom( data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_PutAtop( IDirectFBWindow *thiz,
                         IDirectFBWindow *lower )
{
     IDirectFBWindow_data *lower_data;

     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (!lower)
          return DFB_INVARG;

     lower_data = (IDirectFBWindow_data*) lower->priv;
     if (!lower_data)
          return DFB_DEAD;

     if (!lower_data->window)
          return DFB_DESTROYED;

     dfb_window_putatop( data->window, lower_data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_PutBelow( IDirectFBWindow *thiz,
                           IDirectFBWindow *upper )
{
     IDirectFBWindow_data *upper_data;

     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     if (!upper)
          return DFB_INVARG;

     upper_data = (IDirectFBWindow_data*) upper->priv;
     if (!upper_data)
          return DFB_DEAD;

     if (!upper_data->window)
          return DFB_DESTROYED;

     dfb_window_putbelow( data->window, upper_data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Close( IDirectFBWindow *thiz )
{
     DFBWindowEvent evt;

     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     evt.type = DWET_CLOSE;

     dfb_window_dispatch( data->window, &evt );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Destroy( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!data->window)
          return DFB_DESTROYED;

     dfb_window_deinit( data->window );
     dfb_window_destroy( data->window, true );

     return DFB_OK;
}

DFBResult
IDirectFBWindow_Construct( IDirectFBWindow *thiz,
                           CoreWindow      *window )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBWindow)

     DEBUGMSG( "IDirectFBWindow_Construct: window at %d %d, size %dx%d\n",
                window->x, window->y, window->width, window->height );

     data->ref = 1;
     data->window = window;

     dfb_window_attach( data->window, IDirectFBWindow_React, data );

     dfb_window_init( data->window );

     /* Wait for initial DWET_POSITION_SIZE event */
     while (!data->position_size_event)
          sched_yield();

     thiz->AddRef = IDirectFBWindow_AddRef;
     thiz->Release = IDirectFBWindow_Release;
     thiz->CreateEventBuffer = IDirectFBWindow_CreateEventBuffer;
     thiz->AttachEventBuffer = IDirectFBWindow_AttachEventBuffer;
     thiz->EnableEvents = IDirectFBWindow_EnableEvents;
     thiz->DisableEvents = IDirectFBWindow_DisableEvents;
     thiz->GetID = IDirectFBWindow_GetID;
     thiz->GetPosition = IDirectFBWindow_GetPosition;
     thiz->GetSize = IDirectFBWindow_GetSize;
     thiz->GetSurface = IDirectFBWindow_GetSurface;
     thiz->SetOptions = IDirectFBWindow_SetOptions;
     thiz->GetOptions = IDirectFBWindow_GetOptions;
     thiz->SetColorKey = IDirectFBWindow_SetColorKey;
     thiz->SetOpacity = IDirectFBWindow_SetOpacity;
     thiz->GetOpacity = IDirectFBWindow_GetOpacity;
     thiz->RequestFocus = IDirectFBWindow_RequestFocus;
     thiz->GrabKeyboard = IDirectFBWindow_GrabKeyboard;
     thiz->UngrabKeyboard = IDirectFBWindow_UngrabKeyboard;
     thiz->GrabPointer = IDirectFBWindow_GrabPointer;
     thiz->UngrabPointer = IDirectFBWindow_UngrabPointer;
     thiz->Move = IDirectFBWindow_Move;
     thiz->MoveTo = IDirectFBWindow_MoveTo;
     thiz->Resize = IDirectFBWindow_Resize;
     thiz->SetStackingClass = IDirectFBWindow_SetStackingClass;
     thiz->Raise = IDirectFBWindow_Raise;
     thiz->Lower = IDirectFBWindow_Lower;
     thiz->RaiseToTop = IDirectFBWindow_RaiseToTop;
     thiz->LowerToBottom = IDirectFBWindow_LowerToBottom;
     thiz->PutAtop = IDirectFBWindow_PutAtop;
     thiz->PutBelow = IDirectFBWindow_PutBelow;
     thiz->Close = IDirectFBWindow_Close;
     thiz->Destroy = IDirectFBWindow_Destroy;

     return DFB_OK;
}


/* internals */

static ReactionResult
IDirectFBWindow_React( const void *msg_data,
                       void       *ctx )
{
     const DFBWindowEvent       *evt = (DFBWindowEvent*)msg_data;
     IDirectFBWindow_data       *data = (IDirectFBWindow_data*)ctx;

     if (evt->type == DWET_POSITION_SIZE) {
          if (!data->position_size_event)
               data->position_size_event = DFBMALLOC( sizeof(DFBWindowEvent) );

          *data->position_size_event = *evt;
     }

     if (evt->type == DWET_DESTROYED) {
          data->window = NULL;
          return RS_REMOVE;
     }

     return RS_OK;
}

