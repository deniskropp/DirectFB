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

#include <malloc.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>

#include <windows/idirectfbwindow.h>

#include "idirectfbdisplaylayer.h"
#include "idirectfbsurface.h"
#include "idirectfbsurface_layer.h"

#include <directfb_internals.h>


/*
 * private data struct of IDirectFB
 */
typedef struct {
     int                             ref;    /* reference counter */
     DFBDisplayLayerCooperativeLevel level;  /* current cooperative level */
     DisplayLayer                    *layer; /* pointer to core data */

     IDirectFBSurface                *surface;
} IDirectFBDisplayLayer_data;

     
DFBResult IDirectFBDisplayLayer_bgsurface_listener( CoreSurface  *surface,
                                                    unsigned int  flags,
                                                    void         *ctx );


void IDirectFBDisplayLayer_Destruct( IDirectFBDisplayLayer *thiz )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     /* The background may not be set by us in which case the listener
      * isn't in the listener list, but it doesn't hurt. */
     if (data->layer->bg.image)
          surface_remove_listener( data->layer->bg.image,
                                   IDirectFBDisplayLayer_bgsurface_listener,
                                   thiz );
     
     if (data->surface)
          data->surface->Release( data->surface );

     free( thiz->priv );
     thiz->priv = NULL;
}

DFBResult IDirectFBDisplayLayer_AddRef( IDirectFBDisplayLayer *thiz )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_Release( IDirectFBDisplayLayer *thiz )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0)
          IDirectFBDisplayLayer_Destruct( thiz );

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_SetCooperativeLevel(
                                        IDirectFBDisplayLayer *thiz,
                                        DFBDisplayLayerCooperativeLevel level )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     /* filter all unknown cooperative levels */
     switch (level) {
          case DLSCL_SHARED:
          case DLSCL_EXCLUSIVE:
          case DLSCL_ADMINISTRATIVE:
               break;
          default:
               return DFB_INVARG;
     }

     data->level = level;

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_SetBackgroundMode( IDirectFBDisplayLayer *thiz,
                                 DFBDisplayLayerBackgroundMode background_mode )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (background_mode != data->layer->bg.mode) {
          data->layer->bg.mode = background_mode;

          if (background_mode != DLBM_DONTCARE)
               windowstack_repaint_all( data->layer->windowstack );
     }

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_SetBufferMode( IDirectFBDisplayLayer     *thiz,
                                               DFBDisplayLayerBufferMode  mode )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     switch (mode) {
          case DLBM_BACKSYSTEM:
          case DLBM_BACKVIDEO:
          case DLBM_FRONTONLY:
               return data->layer->SetBufferMode( data->layer, mode );
          default:
     }

     return DFB_INVARG;
}

DFBResult IDirectFBDisplayLayer_SetBackgroundImage( IDirectFBDisplayLayer *thiz,
                                                    IDirectFBSurface *surface )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;
     IDirectFBSurface_data *surface_data =(IDirectFBSurface_data*)surface->priv;

     if (!data)
          return DFB_DEAD;

     if (!surface)
          return DFB_INVARG;

     if (data->layer->bg.image != surface_data->surface) {
          if (data->layer->bg.image)
               surface_remove_listener( data->layer->bg.image,
                                        IDirectFBDisplayLayer_bgsurface_listener,
                                        thiz );

          data->layer->bg.image = surface_data->surface;

          surface_install_listener( surface_data->surface,
                                    IDirectFBDisplayLayer_bgsurface_listener,
                                    (CSN_DESTROY | CSN_FLIP | CSN_SIZEFORMAT),
                                    thiz );
          
          if (data->layer->bg.mode == DLBM_IMAGE)
               windowstack_repaint_all( data->layer->windowstack );
     }

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_SetBackgroundColor( IDirectFBDisplayLayer *thiz,
                                                    __u8 r, __u8 g, __u8 b,
                                                    __u8 a )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->layer->bg.color.r != r  ||
         data->layer->bg.color.g != g  ||
         data->layer->bg.color.b != b  ||
         data->layer->bg.color.a != a)
     {
          data->layer->bg.color.r = r;
          data->layer->bg.color.g = g;
          data->layer->bg.color.b = b;
          data->layer->bg.color.a = a;

          if (data->layer->bg.mode == DLBM_COLOR)
               windowstack_repaint_all( data->layer->windowstack );
     }

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_GetSize( IDirectFBDisplayLayer *thiz,
                                         unsigned int *width,
                                         unsigned int *height )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!width && !height)
          return DFB_INVARG;

     if (width)
          *width = data->layer->width;

     if (height)
          *height = data->layer->height;

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_GetCapabilities( IDirectFBDisplayLayer *thiz,
                                            DFBDisplayLayerCapabilities *caps )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!caps)
          return DFB_INVARG;

     *caps = data->layer->caps;

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_CreateWindow( IDirectFBDisplayLayer *thiz,
                                              DFBWindowDescription *desc,
                                              IDirectFBWindow **window )
{
     CoreWindow *w;
     unsigned int width = 128;
     unsigned int height = 128;
     int posx = 0;
     int posy = 0;
     DFBWindowCapabilities caps = 0;
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (desc->flags & DWDESC_WIDTH)
          width = desc->width;
     if (desc->flags & DWDESC_HEIGHT)
          height = desc->height;
     if (desc->flags & DWDESC_POSX)
          posx = desc->posx;
     if (desc->flags & DWDESC_POSY)
          posy = desc->posy;
     if (desc->flags & DWDESC_CAPS)
          caps = desc->caps;

     if (width > 4096 || height > 4096)
          return DFB_INVARG;

     w = window_create( data->layer->windowstack,
                        posx, posy, width, height, caps );
     if (!w)
          return DFB_FAILURE;

     DFB_ALLOCATE_INTERFACE( *window, IDirectFBWindow );

     return IDirectFBWindow_Construct( *window, w );
}

DFBResult IDirectFBDisplayLayer_GetSurface( IDirectFBDisplayLayer *thiz,
                                            IDirectFBSurface **interface )
{
     DFBResult ret;
     DFBRectangle rect;
     IDirectFBSurface *surface;
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!data->surface) {
          rect.x = 0;
          rect.y = 0;
          rect.w = data->layer->width;
          rect.h = data->layer->height;

          DFB_ALLOCATE_INTERFACE( surface, IDirectFBSurface );

          ret = IDirectFBSurface_Layer_Construct( surface, &rect,
                                                  NULL, data->layer,
                                                  data->layer->surface->caps );
          if (ret) {
               free( surface );
               return ret;
          }

          data->surface = surface;
     }

     data->surface->AddRef( data->surface );

     *interface = data->surface;

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_WarpCursor( IDirectFBDisplayLayer *thiz,
                                            int x, int y )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     layer_cursor_warp( data->layer, x, y );

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_EnableCursor( IDirectFBDisplayLayer *thiz,
                                              int enable )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     layer_cursor_enable( data->layer, enable );

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_GetCursorPosition( IDirectFBDisplayLayer *thiz,
                                                   int *x, int *y )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!x && !y)
          return DFB_INVARG;

     if (x)
          *x = data->layer->windowstack->cx;

     if (y)
          *y = data->layer->windowstack->cy;

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_SetCursorShape( IDirectFBDisplayLayer *thiz,
                                                IDirectFBSurface      *shape,
                                                int                    hot_x,
                                                int                    hot_y )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;
     IDirectFBSurface_data *shape_data = (IDirectFBSurface_data*)shape->priv;

     if (!data)
          return DFB_DEAD;

     if (!shape)
          return DFB_INVARG;

     if (hot_x < 0  ||
         hot_y < 0  ||
         hot_x >= shape_data->surface->width  ||
         hot_y >= shape_data->surface->height)
          return DFB_INVARG;

     return layer_cursor_set_shape( data->layer, shape_data->surface,
                                    hot_x, hot_y );
}

DFBResult IDirectFBDisplayLayer_Construct( IDirectFBDisplayLayer *thiz,
                                           DisplayLayer *layer )
{
     IDirectFBDisplayLayer_data *data;

     data = (IDirectFBDisplayLayer_data*)
            malloc( sizeof(IDirectFBDisplayLayer_data) );

     memset( data, 0, sizeof(IDirectFBDisplayLayer_data) );
     thiz->priv = data;

     data->ref = 1;
     data->layer = layer;

     thiz->AddRef = IDirectFBDisplayLayer_AddRef;
     thiz->Release = IDirectFBDisplayLayer_Release;
     thiz->SetCooperativeLevel = IDirectFBDisplayLayer_SetCooperativeLevel;
     thiz->SetBufferMode = IDirectFBDisplayLayer_SetBufferMode;
     thiz->SetBackgroundMode = IDirectFBDisplayLayer_SetBackgroundMode;
     thiz->SetBackgroundColor = IDirectFBDisplayLayer_SetBackgroundColor;
     thiz->SetBackgroundImage = IDirectFBDisplayLayer_SetBackgroundImage;
     thiz->GetSize = IDirectFBDisplayLayer_GetSize;
     thiz->GetCapabilities = IDirectFBDisplayLayer_GetCapabilities;
     thiz->CreateWindow = IDirectFBDisplayLayer_CreateWindow;
     thiz->GetSurface = IDirectFBDisplayLayer_GetSurface;
     thiz->WarpCursor = IDirectFBDisplayLayer_WarpCursor;
     thiz->EnableCursor = IDirectFBDisplayLayer_EnableCursor;
     thiz->GetCursorPosition = IDirectFBDisplayLayer_GetCursorPosition;
     thiz->SetCursorShape = IDirectFBDisplayLayer_SetCursorShape;

     return DFB_OK;
}


DFBResult IDirectFBDisplayLayer_bgsurface_listener( CoreSurface  *surface,
                                                    unsigned int  flags,
                                                    void         *ctx )
{
     IDirectFBDisplayLayer      *thiz = (IDirectFBDisplayLayer*)ctx;
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return SL_REMOVE;

     if (flags & CSN_DESTROY) {
          DEBUGMSG("IDirectFBDisplayLayer: "
                   "CoreSurface for background vanished.\n");

          data->layer->bg.mode  = DLBM_COLOR;
          data->layer->bg.image = NULL;

          windowstack_repaint_all( data->layer->windowstack );

          return SL_REMOVE;
     }

     if (flags & (CSN_FLIP | CSN_SIZEFORMAT))
          windowstack_repaint_all( data->layer->windowstack );
     
     return SL_OK;
}
