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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sched.h>

#include <sys/time.h>
#include <errno.h>

#include <pthread.h>

#include <directfb.h>

#include <idirectfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/palette.h>
#include <core/state.h>
#include <core/surfaces.h>
#include <core/windows.h>
#include <core/windowstack.h>
#include <core/windows_internal.h> /* FIXME */

#include <display/idirectfbsurface.h>
#include <display/idirectfbsurface_window.h>

#include <input/idirectfbinputbuffer.h>

#include <misc/util.h>
#include <direct/interface.h>
#include <direct/mem.h>

#include <gfx/convert.h>

#include <windows/idirectfbwindow.h>

/*
 * adds an window event to the event queue
 */
static ReactionResult IDirectFBWindow_React( const void *msg_data,
                                             void       *ctx );



typedef struct {
     int                ref;
     CoreWindow        *window;
     CoreLayer         *layer;

     IDirectFBSurface  *surface;

     struct {
          IDirectFBSurface  *shape;
          int                hot_x;
          int                hot_y;
     } cursor;

     Reaction           reaction;

     bool               entered;

     bool               detached;
     bool               destroyed;
} IDirectFBWindow_data;


static void
IDirectFBWindow_Destruct( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     D_DEBUG("IDirectFBWindow_Destruct...\n");

     if (!data->detached) {
          D_DEBUG("IDirectFBWindow_Destruct - detaching...\n");

          dfb_window_detach( data->window, &data->reaction );

          D_DEBUG("IDirectFBWindow_Destruct - detached.\n");
     }

     if (!data->destroyed) {
          D_DEBUG("IDirectFBWindow_Destruct - unrefing...\n");

          dfb_window_unref( data->window );

          D_DEBUG("IDirectFBWindow_Destruct - unref done.\n");
     }

     if (data->surface)
          data->surface->Release( data->surface );

     if (data->cursor.shape)
          data->cursor.shape->Release( data->cursor.shape );

     D_DEBUG("IDirectFBWindow_Destruct - done.\n");

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBWindow_AddRef( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Release( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (--data->ref == 0)
          IDirectFBWindow_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_CreateEventBuffer( IDirectFBWindow       *thiz,
                                   IDirectFBEventBuffer **buffer )
{
     IDirectFBEventBuffer *b;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     DIRECT_ALLOCATE_INTERFACE( b, IDirectFBEventBuffer );

     IDirectFBEventBuffer_Construct( b, NULL, NULL );

     IDirectFBEventBuffer_AttachWindow( b, data->window );

     dfb_window_send_configuration( data->window );

     *buffer = b;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_AttachEventBuffer( IDirectFBWindow       *thiz,
                                   IDirectFBEventBuffer  *buffer )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     IDirectFBEventBuffer_AttachWindow( buffer, data->window );

     dfb_window_send_configuration( data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_EnableEvents( IDirectFBWindow       *thiz,
                              DFBWindowEventType     mask )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
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
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
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
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
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
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
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
                         int             *width,
                         int             *height )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
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
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     if (!surface)
          return DFB_INVARG;

     if (data->window->caps & DWCAPS_INPUTONLY)
          return DFB_UNSUPPORTED;

     if (!data->surface) {
          DFBResult ret;

          DIRECT_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

          ret = IDirectFBSurface_Window_Construct( *surface,
                                                   NULL, NULL, data->window,
                                                   DSCAPS_DOUBLE );
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
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     /* Check arguments */
     if (data->destroyed)
          return DFB_DESTROYED;

     if (options & ~DWOP_ALL)
          return DFB_INVARG;

     if (!(data->window->caps & DWCAPS_ALPHACHANNEL))
          options &= ~DWOP_ALPHACHANNEL;

     /* Set new options */
     dfb_window_set_options( data->window, options );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_GetOptions( IDirectFBWindow  *thiz,
                            DFBWindowOptions *options )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
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
     __u32        key;
     CoreSurface *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     if (data->window->caps & DWCAPS_INPUTONLY)
          return DFB_UNSUPPORTED;

     surface = data->window->surface;

     if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
          key = dfb_palette_search( surface->palette, r, g, b, 0x80 );
     else
          key = dfb_color_to_pixel( surface->format, r, g, b );

     if (data->window->color_key != key) {
          data->window->color_key = key;

          if (data->window->options & DWOP_COLORKEYING)
               dfb_window_repaint( data->window, NULL, DSFLIP_NONE );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_SetColorKeyIndex( IDirectFBWindow *thiz,
                                  unsigned int     index )
{
     __u32 key = index;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     if (data->window->caps & DWCAPS_INPUTONLY)
          return DFB_UNSUPPORTED;

     if (data->window->color_key != key) {
          data->window->color_key = key;

          if (data->window->options & DWOP_COLORKEYING)
               dfb_window_repaint( data->window, NULL, DSFLIP_NONE );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_SetOpaqueRegion( IDirectFBWindow *thiz,
                                 int              x1,
                                 int              y1,
                                 int              x2,
                                 int              y2 )
{
     CoreWindow *window;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     if (x1 > x2 || y1 > y2)
          return DFB_INVAREA;

     window = data->window;

     window->opaque.x1 = x1;
     window->opaque.y1 = y1;
     window->opaque.x2 = x2;
     window->opaque.y2 = y2;

     dfb_region_intersect( &window->opaque,
                           0, 0, window->width - 1, window->height - 1 );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_SetOpacity( IDirectFBWindow *thiz,
                            __u8             opacity )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     if (data->window->opacity != opacity)
          dfb_window_set_opacity( data->window, opacity );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_GetOpacity( IDirectFBWindow *thiz,
                            __u8            *opacity )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     if (!opacity)
          return DFB_INVARG;

     *opacity = data->window->opacity;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_SetCursorShape( IDirectFBWindow  *thiz,
                                IDirectFBSurface *shape,
                                int               hot_x,
                                int               hot_y )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     if (data->cursor.shape) {
          data->cursor.shape->Release( data->cursor.shape );
          data->cursor.shape = NULL;
     }

     if (shape) {
          IDirectFBSurface_data *shape_data;
          CoreSurface           *shape_surface;

          shape_data = (IDirectFBSurface_data*) shape->priv;
          if (!shape_data)
               return DFB_DEAD;

          shape_surface = shape_data->surface;
          if (!shape_surface)
               return DFB_DESTROYED;

          ret = shape->AddRef( shape );
          if (ret)
               return ret;

          data->cursor.shape = shape;
          data->cursor.hot_x = hot_x;
          data->cursor.hot_y = hot_y;

          if (data->entered)
               dfb_windowstack_cursor_set_shape( data->window->stack,
                                                 shape_surface, hot_x, hot_y );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_RequestFocus( IDirectFBWindow *thiz )
{
     CoreWindow *window;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     window = data->window;

     if (window->options & DWOP_GHOST)
          return DFB_UNSUPPORTED;

     if (!window->opacity && !(window->caps & DWCAPS_INPUTONLY))
          return DFB_UNSUPPORTED;

     dfb_window_request_focus( window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_GrabKeyboard( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     return dfb_window_grab_keyboard( data->window );
}

static DFBResult
IDirectFBWindow_UngrabKeyboard( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     return dfb_window_ungrab_keyboard( data->window );
}

static DFBResult
IDirectFBWindow_GrabPointer( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     return dfb_window_grab_pointer( data->window );
}

static DFBResult
IDirectFBWindow_UngrabPointer( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     return dfb_window_ungrab_pointer( data->window );
}

static DFBResult
IDirectFBWindow_GrabKey( IDirectFBWindow            *thiz,
                         DFBInputDeviceKeySymbol     symbol,
                         DFBInputDeviceModifierMask  modifiers )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     return dfb_window_grab_key( data->window, symbol, modifiers );
}

static DFBResult
IDirectFBWindow_UngrabKey( IDirectFBWindow            *thiz,
                           DFBInputDeviceKeySymbol     symbol,
                           DFBInputDeviceModifierMask  modifiers )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     return dfb_window_ungrab_key( data->window, symbol, modifiers );
}

static DFBResult
IDirectFBWindow_Move( IDirectFBWindow *thiz, int dx, int dy )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     if (dx == 0  &&  dy == 0)
          return DFB_OK;

     dfb_window_move( data->window, dx, dy );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_MoveTo( IDirectFBWindow *thiz, int x, int y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     if (data->window->x == x  &&  data->window->y == y)
          return DFB_OK;

     dfb_window_move( data->window,
                      x - data->window->x, y - data->window->y );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Resize( IDirectFBWindow *thiz,
                        int              width,
                        int              height )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     if (width < 1 || width > 4096 || height < 1 || height > 4096)
          return DFB_INVARG;

     if (data->window->width == width  &&  data->window->height == height)
          return DFB_OK;

     return dfb_window_resize( data->window, width, height );
}

static DFBResult
IDirectFBWindow_Raise( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     dfb_window_raise( data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_SetStackingClass( IDirectFBWindow        *thiz,
                                  DFBWindowStackingClass  stacking_class )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
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
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     dfb_window_lower( data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_RaiseToTop( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     dfb_window_raisetotop( data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_LowerToBottom( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     dfb_window_lowertobottom( data->window );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_PutAtop( IDirectFBWindow *thiz,
                         IDirectFBWindow *lower )
{
     IDirectFBWindow_data *lower_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
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

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
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

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     evt.type = DWET_CLOSE;

     dfb_window_post_event( data->window, &evt );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Destroy( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->destroyed)
          return DFB_DESTROYED;

     D_DEBUG("IDirectFBWindow_Destroy\n");

     dfb_window_destroy( data->window );

     return DFB_OK;
}

DFBResult
IDirectFBWindow_Construct( IDirectFBWindow *thiz,
                           CoreWindow      *window,
                           CoreLayer       *layer )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBWindow)

     D_DEBUG( "IDirectFBWindow_Construct: window at %d %d, size %dx%d\n",
                window->x, window->y, window->width, window->height );

     data->ref    = 1;
     data->window = window;
     data->layer  = layer;

     dfb_window_attach( window, IDirectFBWindow_React, data, &data->reaction );

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
     thiz->SetColorKeyIndex = IDirectFBWindow_SetColorKeyIndex;
     thiz->SetOpaqueRegion = IDirectFBWindow_SetOpaqueRegion;
     thiz->SetOpacity = IDirectFBWindow_SetOpacity;
     thiz->GetOpacity = IDirectFBWindow_GetOpacity;
     thiz->SetCursorShape = IDirectFBWindow_SetCursorShape;
     thiz->RequestFocus = IDirectFBWindow_RequestFocus;
     thiz->GrabKeyboard = IDirectFBWindow_GrabKeyboard;
     thiz->UngrabKeyboard = IDirectFBWindow_UngrabKeyboard;
     thiz->GrabPointer = IDirectFBWindow_GrabPointer;
     thiz->UngrabPointer = IDirectFBWindow_UngrabPointer;
     thiz->GrabKey = IDirectFBWindow_GrabKey;
     thiz->UngrabKey = IDirectFBWindow_UngrabKey;
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
     const DFBWindowEvent *evt  = msg_data;
     IDirectFBWindow_data *data = ctx;

     D_HEAVYDEBUG("IDirectFBWindow_React\n");

     switch (evt->type) {
          case DWET_DESTROYED:
               D_DEBUG("IDirectFBWindow_React - window destroyed\n");

               if (!data->destroyed) {
                    data->destroyed = true;

                    D_DEBUG("IDirectFBWindow_React - unrefing...\n");

                    dfb_window_unref( data->window );

                    D_DEBUG("IDirectFBWindow_React - unref done.\n");
               }

               data->detached = true;

               return RS_REMOVE;

          case DWET_LEAVE:
               data->entered = false;
               break;

          case DWET_ENTER:
               data->entered = true;

               if (data->cursor.shape) {
                    IDirectFBSurface_data* shape_data;

                    shape_data = (IDirectFBSurface_data*) data->cursor.shape->priv;
                    if (!shape_data)
                         break;

                    if (!shape_data->surface)
                         break;

                    dfb_windowstack_cursor_set_shape( data->window->stack,
                                                      shape_data->surface,
                                                      data->cursor.hot_x,
                                                      data->cursor.hot_y );
               }

               break;

          case DWET_GOTFOCUS:
          case DWET_LOSTFOCUS:
               IDirectFB_SetAppFocus( idirectfb_singleton,
                                      evt->type == DWET_GOTFOCUS );

          default:
               break;
     }

     return RS_OK;
}

