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

#include "gfx/convert.h"

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



static void
IDirectFBDisplayLayer_Destruct( IDirectFBDisplayLayer *thiz )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     if (data->surface)
          data->surface->Release( data->surface );

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBDisplayLayer_AddRef( IDirectFBDisplayLayer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_Release( IDirectFBDisplayLayer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (--data->ref == 0)
          IDirectFBDisplayLayer_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetID( IDirectFBDisplayLayer *thiz,
                             DFBDisplayLayerID     *id )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!id)
          return DFB_INVARG;

     *id = dfb_layer_id( data->layer );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetDescription( IDirectFBDisplayLayer      *thiz,
                                      DFBDisplayLayerDescription *desc )
{
     DFBDisplayLayerDescription description;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!desc)
          return DFB_INVARG;

     dfb_layer_description( data->layer, &description );

     *desc = description;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetSurface( IDirectFBDisplayLayer  *thiz,
                                  IDirectFBSurface      **interface )
{
     DFBResult ret;
     DFBRectangle rect;
     IDirectFBSurface *surface;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!interface)
          return DFB_INVARG;

     /* FIXME: cooperative level */

     if (!data->surface) {
          CoreSurface *layer_surface = dfb_layer_surface( data->layer );

          rect.x = 0;
          rect.y = 0;
          rect.w = layer_surface->width;
          rect.h = layer_surface->height;

          DFB_ALLOCATE_INTERFACE( surface, IDirectFBSurface );

          ret = IDirectFBSurface_Layer_Construct( surface, &rect,
                                                  NULL, data->layer,
                                                  layer_surface->caps );
          if (ret)
               return ret;

          data->surface = surface;
     }

     data->surface->AddRef( data->surface );

     *interface = data->surface;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_SetCooperativeLevel( IDirectFBDisplayLayer           *thiz,
                                           DFBDisplayLayerCooperativeLevel  level )
{
     DFBResult ret;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == level)
          return DFB_OK;

     switch (level) {
          case DLSCL_SHARED:
          case DLSCL_ADMINISTRATIVE:
               if (data->level == DLSCL_EXCLUSIVE)
                    dfb_layer_release( data->layer, true );
               
               break;

          case DLSCL_EXCLUSIVE:
               ret = dfb_layer_purchase( data->layer );
               if (ret)
                    return ret;

               break;

          default:
               return DFB_INVARG;
     }

     data->level = level;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_SetOpacity( IDirectFBDisplayLayer *thiz,
                                  __u8                   opacity )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     /* FIXME: cooperative level */
     
     return dfb_layer_set_opacity( data->layer, opacity );
}

static DFBResult
IDirectFBDisplayLayer_SetScreenLocation( IDirectFBDisplayLayer *thiz,
                                         float                  x,
                                         float                  y,
                                         float                  width,
                                         float                  height )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (width <= 0 || height <= 0)
          return DFB_INVARG;

     /* FIXME: cooperative level */
     
     return dfb_layer_set_screenlocation( data->layer, x, y, width, height );
}

static DFBResult
IDirectFBDisplayLayer_SetSrcColorKey( IDirectFBDisplayLayer *thiz,
                                      __u8                   r,
                                      __u8                   g,
                                      __u8                   b )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     /* FIXME: cooperative level */
     
     return dfb_layer_set_src_colorkey( data->layer, r, g, b );
}

static DFBResult
IDirectFBDisplayLayer_SetDstColorKey( IDirectFBDisplayLayer *thiz,
                                      __u8                   r,
                                      __u8                   g,
                                      __u8                   b )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     /* FIXME: cooperative level */
     
     return dfb_layer_set_dst_colorkey( data->layer, r, g, b );
}

static DFBResult
IDirectFBDisplayLayer_GetLevel( IDirectFBDisplayLayer *thiz,
                                int                   *level )
{
     DFBResult ret;
     int       lvl;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!level)
          return DFB_INVARG;

     ret = dfb_layer_get_level( data->layer, &lvl );
     if (ret)
          return ret;

     *level = lvl;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_SetLevel( IDirectFBDisplayLayer *thiz,
                                int                    level )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     /* FIXME: cooperative level */
     
     return dfb_layer_set_level( data->layer, level );
}

static DFBResult
IDirectFBDisplayLayer_GetConfiguration( IDirectFBDisplayLayer *thiz,
                                        DFBDisplayLayerConfig *config )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!config)
          return DFB_INVARG;

     return dfb_layer_get_configuration( data->layer, config );
}

static DFBResult
IDirectFBDisplayLayer_TestConfiguration( IDirectFBDisplayLayer      *thiz,
                                         DFBDisplayLayerConfig      *config,
                                         DFBDisplayLayerConfigFlags *failed )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!config)
          return DFB_INVARG;

     return dfb_layer_test_configuration( data->layer, config, failed );
}

static DFBResult
IDirectFBDisplayLayer_SetConfiguration( IDirectFBDisplayLayer *thiz,
                                        DFBDisplayLayerConfig *config )
{
     DFBResult ret;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!config)
          return DFB_INVARG;

     switch (data->level) {
          case DLSCL_EXCLUSIVE:
               break;

          default:
               ret = dfb_layer_lease( data->layer );
               if (ret)
                    return ret;

               ret = dfb_layer_set_configuration( data->layer, config );

               dfb_layer_release( data->layer, false );

               return ret;
     }
     
     return dfb_layer_set_configuration( data->layer, config );
}

static DFBResult
IDirectFBDisplayLayer_SetBackgroundMode( IDirectFBDisplayLayer         *thiz,
                                         DFBDisplayLayerBackgroundMode  background_mode )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     switch (background_mode) {
          case DLBM_DONTCARE:
          case DLBM_COLOR:
          case DLBM_IMAGE:
          case DLBM_TILE:
               break;

          default:
               return DFB_INVARG;
     }
     
     return dfb_layer_set_background_mode( data->layer, background_mode );
}

static DFBResult
IDirectFBDisplayLayer_SetBackgroundImage( IDirectFBDisplayLayer *thiz,
                                          IDirectFBSurface      *surface )
{
     IDirectFBSurface_data *surface_data;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)


     if (!surface)
          return DFB_INVARG;

     surface_data = (IDirectFBSurface_data*)surface->priv;
     if (!surface_data)
          return DFB_DEAD;

     if (!surface_data->surface)
          return DFB_DESTROYED;

     return dfb_layer_set_background_image( data->layer, surface_data->surface );
}

static DFBResult
IDirectFBDisplayLayer_SetBackgroundColor( IDirectFBDisplayLayer *thiz,
                                          __u8 r, __u8 g, __u8 b, __u8 a )
{
     DFBColor color = { a: a, r: r, g: g, b: b };

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     return dfb_layer_set_background_color( data->layer, &color );
}

static DFBResult
IDirectFBDisplayLayer_CreateWindow( IDirectFBDisplayLayer  *thiz,
                                    DFBWindowDescription   *desc,
                                    IDirectFBWindow       **window )
{
     CoreWindow            *w;
     DFBResult              ret;
     unsigned int           width        = 128;
     unsigned int           height       = 128;
     int                    posx         = 0;
     int                    posy         = 0;
     DFBWindowCapabilities  caps         = 0;
     DFBSurfaceCapabilities surface_caps = 0;
     DFBSurfacePixelFormat  format       = DSPF_UNKNOWN;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)


     if (desc->flags & DWDESC_WIDTH)
          width = desc->width;
     if (desc->flags & DWDESC_HEIGHT)
          height = desc->height;
     if (desc->flags & DWDESC_PIXELFORMAT)
          format = desc->pixelformat;
     if (desc->flags & DWDESC_POSX)
          posx = desc->posx;
     if (desc->flags & DWDESC_POSY)
          posy = desc->posy;
     if (desc->flags & DWDESC_CAPS)
          caps = desc->caps;
     if (desc->flags & DWDESC_SURFACE_CAPS)
          caps = desc->surface_caps;

     if ((caps & ~DWCAPS_ALL) || !window)
          return DFB_INVARG;

     if (!width || width > 4096 || !height || height > 4096)
          return DFB_INVARG;

     ret = dfb_layer_create_window( data->layer, posx, posy, width, height,
                                    caps, surface_caps, format, &w );
     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( *window, IDirectFBWindow );

     return IDirectFBWindow_Construct( *window, w );
}

static DFBResult
IDirectFBDisplayLayer_GetWindow( IDirectFBDisplayLayer  *thiz,
                                 DFBWindowID             id,
                                 IDirectFBWindow       **window )
{
     CoreWindow *w;

     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!window)
          return DFB_INVARG;

     /* FIXME: cooperative level? */
     
     w = dfb_layer_find_window( data->layer, id );
     if (!w)
          return DFB_IDNOTFOUND;

     DFB_ALLOCATE_INTERFACE( *window, IDirectFBWindow );

     return IDirectFBWindow_Construct( *window, w );
}

static DFBResult
IDirectFBDisplayLayer_EnableCursor( IDirectFBDisplayLayer *thiz, int enable )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     return dfb_layer_cursor_enable( data->layer, enable );
}

static DFBResult
IDirectFBDisplayLayer_GetCursorPosition( IDirectFBDisplayLayer *thiz,
                                         int *x, int *y )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!x && !y)
          return DFB_INVARG;

     return dfb_layer_get_cursor_position( data->layer, x, y );
}

static DFBResult
IDirectFBDisplayLayer_WarpCursor( IDirectFBDisplayLayer *thiz, int x, int y )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     return dfb_layer_cursor_warp( data->layer, x, y );
}

static DFBResult
IDirectFBDisplayLayer_SetCursorAcceleration( IDirectFBDisplayLayer *thiz,
                                             int                    numerator,
                                             int                    denominator,
                                             int                    threshold )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (numerator < 0  ||  denominator < 1  ||  threshold < 0)
          return DFB_INVARG;

     return dfb_layer_cursor_set_acceleration( data->layer, numerator,
                                               denominator, threshold );
}

static DFBResult
IDirectFBDisplayLayer_SetCursorShape( IDirectFBDisplayLayer *thiz,
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

     return dfb_layer_cursor_set_shape( data->layer, shape_data->surface,
                                        hot_x, hot_y );
}

static DFBResult
IDirectFBDisplayLayer_SetCursorOpacity( IDirectFBDisplayLayer *thiz,
                                        __u8                   opacity )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     return dfb_layer_cursor_set_opacity( data->layer, opacity );
}

static DFBResult
IDirectFBDisplayLayer_GetColorAdjustment( IDirectFBDisplayLayer *thiz,
                                          DFBColorAdjustment    *adj )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!adj)
          return DFB_INVARG;

     return dfb_layer_get_coloradjustment( data->layer, adj );
}

static DFBResult
IDirectFBDisplayLayer_SetColorAdjustment( IDirectFBDisplayLayer *thiz,
                                          DFBColorAdjustment    *adj )
{
     INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!adj || !adj->flags)
          return DFB_INVARG;

     /* FIXME: cooperative level */
     
     return dfb_layer_set_coloradjustment( data->layer, adj );
}

DFBResult
IDirectFBDisplayLayer_Construct( IDirectFBDisplayLayer *thiz,
                                 DisplayLayer          *layer )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDisplayLayer)

     data->ref = 1;
     data->layer = layer;

     thiz->AddRef = IDirectFBDisplayLayer_AddRef;
     thiz->Release = IDirectFBDisplayLayer_Release;
     thiz->GetID = IDirectFBDisplayLayer_GetID;
     thiz->GetDescription = IDirectFBDisplayLayer_GetDescription;
     thiz->GetSurface = IDirectFBDisplayLayer_GetSurface;
     thiz->SetCooperativeLevel = IDirectFBDisplayLayer_SetCooperativeLevel;
     thiz->SetOpacity = IDirectFBDisplayLayer_SetOpacity;
     thiz->SetScreenLocation = IDirectFBDisplayLayer_SetScreenLocation;
     thiz->SetSrcColorKey = IDirectFBDisplayLayer_SetSrcColorKey;
     thiz->SetDstColorKey = IDirectFBDisplayLayer_SetDstColorKey;
     thiz->GetLevel = IDirectFBDisplayLayer_GetLevel;
     thiz->SetLevel = IDirectFBDisplayLayer_SetLevel;
     thiz->GetConfiguration = IDirectFBDisplayLayer_GetConfiguration;
     thiz->TestConfiguration = IDirectFBDisplayLayer_TestConfiguration;
     thiz->SetConfiguration = IDirectFBDisplayLayer_SetConfiguration;
     thiz->SetBackgroundMode = IDirectFBDisplayLayer_SetBackgroundMode;
     thiz->SetBackgroundColor = IDirectFBDisplayLayer_SetBackgroundColor;
     thiz->SetBackgroundImage = IDirectFBDisplayLayer_SetBackgroundImage;
     thiz->GetColorAdjustment = IDirectFBDisplayLayer_GetColorAdjustment;
     thiz->SetColorAdjustment = IDirectFBDisplayLayer_SetColorAdjustment;
     thiz->CreateWindow = IDirectFBDisplayLayer_CreateWindow;
     thiz->GetWindow = IDirectFBDisplayLayer_GetWindow;
     thiz->WarpCursor = IDirectFBDisplayLayer_WarpCursor;
     thiz->SetCursorAcceleration = IDirectFBDisplayLayer_SetCursorAcceleration;
     thiz->EnableCursor = IDirectFBDisplayLayer_EnableCursor;
     thiz->GetCursorPosition = IDirectFBDisplayLayer_GetCursorPosition;
     thiz->SetCursorShape = IDirectFBDisplayLayer_SetCursorShape;
     thiz->SetCursorOpacity = IDirectFBDisplayLayer_SetCursorOpacity;

     return DFB_OK;
}

