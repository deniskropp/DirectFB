/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <malloc.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>

#include <display/idirectfbsurface.h>
#include <display/idirectfbsurface_window.h>
#include "idirectfbwindow.h"

#include <directfb_internals.h>


void IDirectFBWindow_Destruct( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (data->surface)
          data->surface->Release( data->surface );

     window_remove( data->window );
     window_destroy( data->window );

     free( data );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

DFBResult IDirectFBWindow_AddRef( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

DFBResult IDirectFBWindow_Release( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBWindow_Destruct( thiz );
     }

     return DFB_OK;
}

DFBResult IDirectFBWindow_GetPosition( IDirectFBWindow *thiz, int *x, int *y )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!x && !y)
          return DFB_INVARG;

     if (x)
          *x = data->window->x;

     if (y)
          *y = data->window->y;

     return DFB_OK;
}

DFBResult IDirectFBWindow_GetSize( IDirectFBWindow *thiz, unsigned int *width,
                                                          unsigned int *height )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!width && !height)
          return DFB_INVARG;

     if (width)
          *width = data->window->width;

     if (height)
          *height = data->window->height;

     return DFB_OK;
}

DFBResult IDirectFBWindow_GetSurface( IDirectFBWindow *thiz,
                                      IDirectFBSurface **surface )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!surface)
          return DFB_INVARG;

     if (!data->surface) {
          DFBResult ret;

          DFB_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

          ret = IDirectFBSurface_Window_Construct( *surface, NULL, NULL,
                                                   data->window, 0 );
          if (ret) {
               free( *surface );
               return ret;
          }

          data->surface = *surface;
     }
     else
          *surface = data->surface;

     data->surface->AddRef( data->surface );

     return DFB_OK;
}

DFBResult IDirectFBWindow_SetOpacity( IDirectFBWindow *thiz, __u8 opacity )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->window->opacity != opacity)
          return window_set_opacity( data->window, opacity );

     return DFB_OK;
}

DFBResult IDirectFBWindow_GetOpacity( IDirectFBWindow *thiz, __u8 *opacity )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     *opacity = data->window->opacity;

     return DFB_OK;
}

DFBResult IDirectFBWindow_RequestFocus( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_request_focus( data->window );
}

DFBResult IDirectFBWindow_GrabKeyboard( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_grab_keyboard( data->window );
}

DFBResult IDirectFBWindow_UngrabKeyboard( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_ungrab_keyboard( data->window );
}

DFBResult IDirectFBWindow_GrabPointer( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_grab_pointer( data->window );
}

DFBResult IDirectFBWindow_UngrabPointer( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_ungrab_pointer( data->window );
}

DFBResult IDirectFBWindow_Move( IDirectFBWindow *thiz, int dx, int dy )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (dx == 0  &&  dy == 0)
          return DFB_OK;

     return window_move( data->window, dx, dy );
}

DFBResult IDirectFBWindow_MoveTo( IDirectFBWindow *thiz, int x, int y )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->window->x == x  &&  data->window->y == y)
          return DFB_OK;

     return window_move( data->window,
                         x - data->window->x, y - data->window->y );
}

DFBResult IDirectFBWindow_Resize( IDirectFBWindow *thiz,
                                  unsigned int width, unsigned int height )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->window->width == width  &&  data->window->height == height)
          return DFB_OK;

     window_resize( data->window, width, height );

     if (data->surface) {
          IDirectFBSurface_data *surface_data =
                                    (IDirectFBSurface_data*)data->surface->priv;

          if (surface_data->state.clip.x2 <= data->window->surface->width) {
               surface_data->state.clip.x2 = data->window->surface->width - 1;
               surface_data->state.modified |= SMF_CLIP;
          }
          if (surface_data->state.clip.y2 <= data->window->surface->height) {
               surface_data->state.clip.y2 = data->window->surface->height - 1;
               surface_data->state.modified |= SMF_CLIP;
          }

          surface_data->req_rect.w = data->window->surface->width;
          surface_data->req_rect.h = data->window->surface->height;
          surface_data->clip_rect.w = data->window->surface->width;
          surface_data->clip_rect.h = data->window->surface->height;
     }

     return DFB_OK;
}

DFBResult IDirectFBWindow_Raise( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_raise( data->window );
}

DFBResult IDirectFBWindow_Lower( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_lower( data->window );
}

DFBResult IDirectFBWindow_RaiseToTop( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_raisetotop( data->window );
}

DFBResult IDirectFBWindow_LowerToBottom( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_lowertobottom( data->window );
}

DFBResult IDirectFBWindow_WaitForEvent( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_waitforevent( data->window );
}

DFBResult IDirectFBWindow_GetEvent( IDirectFBWindow *thiz,
                                    DFBWindowEvent *event )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_getevent( data->window, event );
}

DFBResult IDirectFBWindow_PeekEvent( IDirectFBWindow *thiz,
                                     DFBWindowEvent *event )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return window_peekevent( data->window, event );
}

DFBResult IDirectFBWindow_Construct( IDirectFBWindow *thiz,
                                     CoreWindow *window )
{
     IDirectFBWindow_data *data;

     DEBUGMSG( "IDirectFBWindow_Construct: window at %d %d, size %dx%d\n",
                window->x, window->y, window->width, window->height );

     data = (IDirectFBWindow_data*) calloc( 1, sizeof(IDirectFBWindow_data) );

     thiz->priv = data;

     data->ref = 1;
     data->window = window;

     thiz->AddRef = IDirectFBWindow_AddRef;
     thiz->Release = IDirectFBWindow_Release;
     thiz->GetPosition = IDirectFBWindow_GetPosition;
     thiz->GetSize = IDirectFBWindow_GetSize;
     thiz->GetSurface = IDirectFBWindow_GetSurface;
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
     thiz->Raise = IDirectFBWindow_Raise;
     thiz->Lower = IDirectFBWindow_Lower;
     thiz->RaiseToTop = IDirectFBWindow_RaiseToTop;
     thiz->LowerToBottom = IDirectFBWindow_LowerToBottom;
     thiz->WaitForEvent = IDirectFBWindow_WaitForEvent;
     thiz->GetEvent = IDirectFBWindow_GetEvent;
     thiz->PeekEvent = IDirectFBWindow_PeekEvent;

     return DFB_OK;
}

