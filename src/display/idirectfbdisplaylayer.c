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

#include "directfb.h"
#include "directfb_internals.h"

#include "core/core.h"
#include "core/coredefs.h"
#include "core/coretypes.h"

#include "core/surfaces.h"
#include "core/gfxcard.h"
#include "core/layers.h"
#include "core/state.h"
#include "core/windows.h"

#include "windows/idirectfbwindow.h"

#include "idirectfbdisplaylayer.h"
#include "idirectfbsurface.h"
#include "idirectfbsurface_layer.h"

#include "misc/mem.h"

/*
 * private data struct of IDirectFB
 */
typedef struct {
     int                             ref;    /* reference counter */
     DFBDisplayLayerCooperativeLevel level;  /* current cooperative level */
     DisplayLayer                    *layer; /* pointer to core data */

     IDirectFBSurface                *surface;
} IDirectFBDisplayLayer_data;


ReactionResult IDirectFBDisplayLayer_bgsurface_listener( const void *msg_data,
                                                         void       *ctx );


void IDirectFBDisplayLayer_Destruct( IDirectFBDisplayLayer *thiz )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     /* The background may not be set by us in which case the listener
      * isn't in the listener list, but it doesn't hurt. */
     if (data->layer->bg.image)
          reactor_detach( data->layer->bg.image->reactor,
                          IDirectFBDisplayLayer_bgsurface_listener, thiz );

     if (data->surface)
          data->surface->Release( data->surface );

     DFBFREE( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     DFBFREE( thiz );
#endif
}

DFBResult IDirectFBDisplayLayer_AddRef( IDirectFBDisplayLayer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     data->ref++;

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_Release( IDirectFBDisplayLayer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (--data->ref == 0)
          IDirectFBDisplayLayer_Destruct( thiz );

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_GetCapabilities( IDirectFBDisplayLayer *thiz,
                                            DFBDisplayLayerCapabilities *caps )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!caps)
          return DFB_INVARG;

     *caps = data->layer->caps;

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_GetSurface( IDirectFBDisplayLayer *thiz,
                                            IDirectFBSurface **interface )
{
     DFBResult ret;
     DFBRectangle rect;
     IDirectFBSurface *surface;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!interface)
          return DFB_INVARG;

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
               DFBFREE( surface );
               return ret;
          }

          data->surface = surface;
     }

     data->surface->AddRef( data->surface );

     *interface = data->surface;

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_SetCooperativeLevel(
                                        IDirectFBDisplayLayer *thiz,
                                        DFBDisplayLayerCooperativeLevel level )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

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

DFBResult IDirectFBDisplayLayer_SetOpacity( IDirectFBDisplayLayer *thiz,
                                            __u8                   opacity )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     return data->layer->SetOpacity( data->layer, opacity );
}

DFBResult IDirectFBDisplayLayer_SetScreenLocation( IDirectFBDisplayLayer *thiz,
                                                   float                  x,
                                                   float                  y,
                                                   float                  width,
                                                   float                  height )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (width <= 0 || height <= 0)
          return DFB_INVARG;

     return data->layer->SetScreenLocation( data->layer, x, y, width, height );
}

DFBResult IDirectFBDisplayLayer_SetColorKey( IDirectFBDisplayLayer *thiz,
                                             __u32                  key )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     return data->layer->SetColorKey( data->layer, key );
}

DFBResult IDirectFBDisplayLayer_GetConfiguration( IDirectFBDisplayLayer *thiz,
                                                  DFBDisplayLayerConfig *config )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!config)
          return DFB_INVARG;

     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT |
                           DLCONF_BUFFERMODE | DLCONF_OPTIONS;
     config->width       = data->layer->width;
     config->height      = data->layer->height;
     config->pixelformat = data->layer->surface->format;
     config->buffermode  = data->layer->buffermode;
     config->options     = data->layer->options;

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_TestConfiguration(
                                            IDirectFBDisplayLayer      *thiz,
                                            DFBDisplayLayerConfig      *config,
                                            DFBDisplayLayerConfigFlags *failed )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!config)
          return DFB_INVARG;

     return data->layer->TestConfiguration( data->layer, config, failed );
}

DFBResult IDirectFBDisplayLayer_SetConfiguration( IDirectFBDisplayLayer *thiz,
                                                  DFBDisplayLayerConfig *config )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!config)
          return DFB_INVARG;

     return data->layer->SetConfiguration( data->layer, config );
}

DFBResult IDirectFBDisplayLayer_SetBackgroundMode( IDirectFBDisplayLayer *thiz,
                                 DFBDisplayLayerBackgroundMode background_mode )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     switch (background_mode) {
          case DLBM_DONTCARE:
          case DLBM_COLOR:
          case DLBM_IMAGE:
               if (background_mode != data->layer->bg.mode) {
                    if (background_mode == DLBM_IMAGE && !data->layer->bg.image)
                         return DFB_MISSINGIMAGE;

                    data->layer->bg.mode = background_mode;

                    if (background_mode != DLBM_DONTCARE)
                         windowstack_repaint_all( data->layer->windowstack );
               }
               return DFB_OK;
     }

     return DFB_INVARG;
}

DFBResult IDirectFBDisplayLayer_SetBackgroundImage( IDirectFBDisplayLayer *thiz,
                                                    IDirectFBSurface *surface )
{
     IDirectFBSurface_data *surface_data;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)


     if (!surface)
          return DFB_INVARG;

     surface_data = (IDirectFBSurface_data*)surface->priv;

     if (data->layer->bg.image != surface_data->surface) {
          if (data->layer->bg.image)
               reactor_detach( data->layer->bg.image->reactor,
                               IDirectFBDisplayLayer_bgsurface_listener, thiz );

          data->layer->bg.image = surface_data->surface;

          reactor_attach( data->layer->bg.image->reactor,
                          IDirectFBDisplayLayer_bgsurface_listener, thiz );

          if (data->layer->bg.mode == DLBM_IMAGE)
               windowstack_repaint_all( data->layer->windowstack );
     }

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_SetBackgroundColor( IDirectFBDisplayLayer *thiz,
                                                    __u8 r, __u8 g, __u8 b,
                                                    __u8 a )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

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

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)


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

     if (!width || width > 4096 || !height || height > 4096)
          return DFB_INVARG;

     w = window_create( data->layer->windowstack,
                        posx, posy, width, height, caps );
     if (!w)
          return DFB_FAILURE;

     DFB_ALLOCATE_INTERFACE( *window, IDirectFBWindow );

     return IDirectFBWindow_Construct( *window, w );
}

DFBResult IDirectFBDisplayLayer_WarpCursor( IDirectFBDisplayLayer *thiz,
                                            int x, int y )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     layer_cursor_warp( data->layer, x, y );

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_EnableCursor( IDirectFBDisplayLayer *thiz,
                                              int enable )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     layer_cursor_enable( data->layer, enable );

     return DFB_OK;
}

DFBResult IDirectFBDisplayLayer_GetCursorPosition( IDirectFBDisplayLayer *thiz,
                                                   int *x, int *y )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

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
     IDirectFBSurface_data *shape_data;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!shape)
          return DFB_INVARG;

     shape_data = (IDirectFBSurface_data*)shape->priv;

     if (hot_x < 0  ||
         hot_y < 0  ||
         hot_x >= shape_data->surface->width  ||
         hot_y >= shape_data->surface->height)
          return DFB_INVARG;

     return layer_cursor_set_shape( data->layer, shape_data->surface,
                                    hot_x, hot_y );
}

DFBResult IDirectFBDisplayLayer_SetCursorOpacity( IDirectFBDisplayLayer *thiz,
                                                  __u8                   opacity )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     return layer_cursor_set_opacity( data->layer, opacity );
}

DFBResult IDirectFBDisplayLayer_Construct( IDirectFBDisplayLayer *thiz,
                                           DisplayLayer *layer )
{
     IDirectFBDisplayLayer_data *data;

     if (!thiz->priv)
          thiz->priv = DFBCALLOC( 1, sizeof(IDirectFBDisplayLayer_data) );

     data = (IDirectFBDisplayLayer_data*)(thiz->priv);

     data->ref = 1;
     data->layer = layer;

     thiz->AddRef = IDirectFBDisplayLayer_AddRef;
     thiz->Release = IDirectFBDisplayLayer_Release;
     thiz->GetCapabilities = IDirectFBDisplayLayer_GetCapabilities;
     thiz->GetSurface = IDirectFBDisplayLayer_GetSurface;
     thiz->SetCooperativeLevel = IDirectFBDisplayLayer_SetCooperativeLevel;
     thiz->SetOpacity = IDirectFBDisplayLayer_SetOpacity;
     thiz->SetScreenLocation = IDirectFBDisplayLayer_SetScreenLocation;
     thiz->SetColorKey = IDirectFBDisplayLayer_SetColorKey;
     thiz->GetConfiguration = IDirectFBDisplayLayer_GetConfiguration;
     thiz->TestConfiguration = IDirectFBDisplayLayer_TestConfiguration;
     thiz->SetConfiguration = IDirectFBDisplayLayer_SetConfiguration;
     thiz->SetBackgroundMode = IDirectFBDisplayLayer_SetBackgroundMode;
     thiz->SetBackgroundColor = IDirectFBDisplayLayer_SetBackgroundColor;
     thiz->SetBackgroundImage = IDirectFBDisplayLayer_SetBackgroundImage;
     thiz->CreateWindow = IDirectFBDisplayLayer_CreateWindow;
     thiz->WarpCursor = IDirectFBDisplayLayer_WarpCursor;
     thiz->EnableCursor = IDirectFBDisplayLayer_EnableCursor;
     thiz->GetCursorPosition = IDirectFBDisplayLayer_GetCursorPosition;
     thiz->SetCursorShape = IDirectFBDisplayLayer_SetCursorShape;
     thiz->SetCursorOpacity = IDirectFBDisplayLayer_SetCursorOpacity;

     return DFB_OK;
}


ReactionResult IDirectFBDisplayLayer_bgsurface_listener( const void *msg_data,
                                                         void         *ctx )
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*)msg_data;
     IDirectFBDisplayLayer      *thiz = (IDirectFBDisplayLayer*)ctx;
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (!data)
          return RS_REMOVE;

     if (notification->flags & CSNF_DESTROY) {
          DEBUGMSG("IDirectFBDisplayLayer: "
                   "CoreSurface for background vanished.\n");

          data->layer->bg.mode  = DLBM_COLOR;
          data->layer->bg.image = NULL;

          windowstack_repaint_all( data->layer->windowstack );

          return RS_REMOVE;
     }

     if (notification->flags & (CSNF_FLIP | CSNF_SIZEFORMAT))
          windowstack_repaint_all( data->layer->windowstack );

     return RS_OK;
}
