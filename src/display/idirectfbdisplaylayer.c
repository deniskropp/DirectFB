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

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/surfaces.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/state.h>
#include <core/windows.h>
#include <core/windowstack.h>

#include <windows/idirectfbwindow.h>

#include <gfx/convert.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include "idirectfbdisplaylayer.h"
#include "idirectfbscreen.h"
#include "idirectfbsurface.h"
#include "idirectfbsurface_layer.h"

/*
 * private data struct of IDirectFB
 */
typedef struct {
     int                              ref;     /* reference counter */
     DFBDisplayLayerCooperativeLevel  level;   /* current cooperative level */
     CoreScreen                      *screen;  /* layer's screen */
     CoreLayer                       *layer;   /* core layer data */
     CoreLayerContext                *context; /* shared or exclusive context */
     CoreLayerRegion                 *region;  /* primary region of the context */
     CoreWindowStack                 *stack;   /* stack of shared context */
} IDirectFBDisplayLayer_data;



static void
IDirectFBDisplayLayer_Destruct( IDirectFBDisplayLayer *thiz )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     dfb_layer_region_unref( data->region );
     dfb_layer_context_unref( data->context );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBDisplayLayer_AddRef( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_Release( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (--data->ref == 0)
          IDirectFBDisplayLayer_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetID( IDirectFBDisplayLayer *thiz,
                             DFBDisplayLayerID     *id )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!id)
          return DFB_INVARG;

     *id = dfb_layer_id_translated( data->layer );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetDescription( IDirectFBDisplayLayer      *thiz,
                                      DFBDisplayLayerDescription *desc )
{
     DFBDisplayLayerDescription description;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!desc)
          return DFB_INVARG;

     dfb_layer_get_description( data->layer, &description );

     *desc = description;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetSurface( IDirectFBDisplayLayer  *thiz,
                                  IDirectFBSurface      **interface )
{
     DFBResult         ret;
     CoreLayerRegion  *region;
     IDirectFBSurface *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!interface)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED) {
          D_WARN( "letting unprivileged IDirectFBDisplayLayer::GetSurface() "
                   "call pass until cooperative level handling is finished" );
     }

     ret = dfb_layer_context_get_primary_region( data->context, true, &region );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( surface, IDirectFBSurface );

     ret = IDirectFBSurface_Layer_Construct( surface, NULL, NULL,
                                             region, DSCAPS_NONE );

     *interface = ret ? NULL : surface;

     dfb_layer_region_unref( region );

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_GetScreen( IDirectFBDisplayLayer  *thiz,
                                 IDirectFBScreen       **interface )
{
     DFBResult        ret;
     IDirectFBScreen *screen;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!interface)
          return DFB_INVARG;

     DIRECT_ALLOCATE_INTERFACE( screen, IDirectFBScreen );

     ret = IDirectFBScreen_Construct( screen, data->screen );

     *interface = ret ? NULL : screen;

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_SetCooperativeLevel( IDirectFBDisplayLayer           *thiz,
                                           DFBDisplayLayerCooperativeLevel  level )
{
     DFBResult         ret;
     CoreLayerContext *context;
     CoreLayerRegion  *region;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == level)
          return DFB_OK;

     switch (level) {
          case DLSCL_SHARED:
          case DLSCL_ADMINISTRATIVE:
               if (data->level == DLSCL_EXCLUSIVE) {
                    ret = dfb_layer_get_primary_context( data->layer, false, &context );
                    if (ret)
                         return ret;

                    ret = dfb_layer_context_get_primary_region( context, true, &region );
                    if (ret) {
                         dfb_layer_context_unref( context );
                         return ret;
                    }

                    dfb_layer_region_unref( data->region );
                    dfb_layer_context_unref( data->context );

                    data->context = context;
                    data->region  = region;
               }

               break;

          case DLSCL_EXCLUSIVE:
               ret = dfb_layer_create_context( data->layer, &context );
               if (ret)
                    return ret;

               ret = dfb_layer_activate_context( data->layer, context );
               if (ret) {
                    dfb_layer_context_unref( context );
                    return ret;
               }

               ret = dfb_layer_context_get_primary_region( context, true, &region );
               if (ret) {
                    dfb_layer_context_unref( context );
                    return ret;
               }

               dfb_layer_region_unref( data->region );
               dfb_layer_context_unref( data->context );

               data->context = context;
               data->region  = region;

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
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_opacity( data->context, opacity );
}

static DFBResult
IDirectFBDisplayLayer_GetCurrentOutputField( IDirectFBDisplayLayer *thiz, int *field )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     return dfb_layer_get_current_output_field( data->layer, field );
}

static DFBResult
IDirectFBDisplayLayer_SetFieldParity( IDirectFBDisplayLayer *thiz, int field )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level != DLSCL_EXCLUSIVE)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_field_parity( data->context, field );
}

static DFBResult
IDirectFBDisplayLayer_SetSourceRectangle( IDirectFBDisplayLayer *thiz,
                                          int                    x,
                                          int                    y,
                                          int                    width,
                                          int                    height )
{
     DFBRectangle source = { x, y, width, height };

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (x < 0 || y < 0 || width <= 0 || height <= 0)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_sourcerectangle( data->context, &source );
}

static DFBResult
IDirectFBDisplayLayer_SetScreenLocation( IDirectFBDisplayLayer *thiz,
                                         float                  x,
                                         float                  y,
                                         float                  width,
                                         float                  height )
{
     DFBLocation location = { x, y, width, height };

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (width <= 0 || height <= 0)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_screenlocation( data->context, &location );
}

static DFBResult
IDirectFBDisplayLayer_SetSrcColorKey( IDirectFBDisplayLayer *thiz,
                                      __u8                   r,
                                      __u8                   g,
                                      __u8                   b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_src_colorkey( data->context, r, g, b );
}

static DFBResult
IDirectFBDisplayLayer_SetDstColorKey( IDirectFBDisplayLayer *thiz,
                                      __u8                   r,
                                      __u8                   g,
                                      __u8                   b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_dst_colorkey( data->context, r, g, b );
}

static DFBResult
IDirectFBDisplayLayer_GetLevel( IDirectFBDisplayLayer *thiz,
                                int                   *level )
{
     DFBResult ret;
     int       lvl;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

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
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_set_level( data->layer, level );
}

static DFBResult
IDirectFBDisplayLayer_GetConfiguration( IDirectFBDisplayLayer *thiz,
                                        DFBDisplayLayerConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!config)
          return DFB_INVARG;

     return dfb_layer_context_get_configuration( data->context, config );
}

static DFBResult
IDirectFBDisplayLayer_TestConfiguration( IDirectFBDisplayLayer      *thiz,
                                         DFBDisplayLayerConfig      *config,
                                         DFBDisplayLayerConfigFlags *failed )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!config)
          return DFB_INVARG;

     if (((config->flags & DLCONF_WIDTH) && (config->width < 0)) ||
         ((config->flags & DLCONF_HEIGHT) && (config->height < 0)))
          return DFB_INVARG;

     return dfb_layer_context_test_configuration( data->context, config, failed );
}

static DFBResult
IDirectFBDisplayLayer_SetConfiguration( IDirectFBDisplayLayer *thiz,
                                        DFBDisplayLayerConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!config)
          return DFB_INVARG;

     if (((config->flags & DLCONF_WIDTH) && (config->width < 0)) ||
         ((config->flags & DLCONF_HEIGHT) && (config->height < 0)))
          return DFB_INVARG;

     switch (data->level) {
          case DLSCL_EXCLUSIVE:
          case DLSCL_ADMINISTRATIVE:
               return dfb_layer_context_set_configuration( data->context, config );

          default:
               break;
     }

     return DFB_ACCESSDENIED;
}

static DFBResult
IDirectFBDisplayLayer_SetBackgroundMode( IDirectFBDisplayLayer         *thiz,
                                         DFBDisplayLayerBackgroundMode  background_mode )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level != DLSCL_ADMINISTRATIVE)
          return DFB_ACCESSDENIED;

     switch (background_mode) {
          case DLBM_DONTCARE:
          case DLBM_COLOR:
          case DLBM_IMAGE:
          case DLBM_TILE:
               break;

          default:
               return DFB_INVARG;
     }

     return dfb_windowstack_set_background_mode( data->stack, background_mode );
}

static DFBResult
IDirectFBDisplayLayer_SetBackgroundImage( IDirectFBDisplayLayer *thiz,
                                          IDirectFBSurface      *surface )
{
     IDirectFBSurface_data *surface_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)


     if (!surface)
          return DFB_INVARG;

     if (data->level != DLSCL_ADMINISTRATIVE)
          return DFB_ACCESSDENIED;

     surface_data = (IDirectFBSurface_data*)surface->priv;
     if (!surface_data)
          return DFB_DEAD;

     if (!surface_data->surface)
          return DFB_DESTROYED;

     return dfb_windowstack_set_background_image( data->stack,
                                                  surface_data->surface );
}

static DFBResult
IDirectFBDisplayLayer_SetBackgroundColor( IDirectFBDisplayLayer *thiz,
                                          __u8 r, __u8 g, __u8 b, __u8 a )
{
     DFBColor color = { a: a, r: r, g: g, b: b };

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level != DLSCL_ADMINISTRATIVE)
          return DFB_ACCESSDENIED;

     return dfb_windowstack_set_background_color( data->stack, &color );
}

static DFBResult
IDirectFBDisplayLayer_CreateWindow( IDirectFBDisplayLayer       *thiz,
                                    const DFBWindowDescription  *desc,
                                    IDirectFBWindow            **window )
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

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)


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
          surface_caps = desc->surface_caps;

     if ((caps & ~DWCAPS_ALL) || !window)
          return DFB_INVARG;

     if (width < 1 || width > 4096 || height < 1 || height > 4096)
          return DFB_INVARG;

     ret = dfb_layer_context_create_window( data->context,
                                            posx, posy, width, height, caps,
                                            surface_caps, format, &w );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( *window, IDirectFBWindow );

     return IDirectFBWindow_Construct( *window, w, true );
}

static DFBResult
IDirectFBDisplayLayer_GetWindow( IDirectFBDisplayLayer  *thiz,
                                 DFBWindowID             id,
                                 IDirectFBWindow       **window )
{
     CoreWindow *w;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!window)
          return DFB_INVARG;

     if (data->level != DLSCL_ADMINISTRATIVE)
          return DFB_ACCESSDENIED;

     w = dfb_layer_context_find_window( data->context, id );
     if (!w)
          return DFB_IDNOTFOUND;

     DIRECT_ALLOCATE_INTERFACE( *window, IDirectFBWindow );

     return IDirectFBWindow_Construct( *window, w, false );
}

static DFBResult
IDirectFBDisplayLayer_EnableCursor( IDirectFBDisplayLayer *thiz, int enable )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level != DLSCL_ADMINISTRATIVE)
          return DFB_ACCESSDENIED;

     return dfb_windowstack_cursor_enable( data->stack, enable );
}

static DFBResult
IDirectFBDisplayLayer_GetCursorPosition( IDirectFBDisplayLayer *thiz,
                                         int *x, int *y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!x && !y)
          return DFB_INVARG;

     return dfb_windowstack_get_cursor_position( data->stack, x, y );
}

static DFBResult
IDirectFBDisplayLayer_WarpCursor( IDirectFBDisplayLayer *thiz, int x, int y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level != DLSCL_ADMINISTRATIVE)
          return DFB_ACCESSDENIED;

     return dfb_windowstack_cursor_warp( data->stack, x, y );
}

static DFBResult
IDirectFBDisplayLayer_SetCursorAcceleration( IDirectFBDisplayLayer *thiz,
                                             int                    numerator,
                                             int                    denominator,
                                             int                    threshold )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (numerator < 0  ||  denominator < 1  ||  threshold < 0)
          return DFB_INVARG;

     if (data->level != DLSCL_ADMINISTRATIVE)
          return DFB_ACCESSDENIED;

     return dfb_windowstack_cursor_set_acceleration( data->stack, numerator,
                                                     denominator, threshold );
}

static DFBResult
IDirectFBDisplayLayer_SetCursorShape( IDirectFBDisplayLayer *thiz,
                                      IDirectFBSurface      *shape,
                                      int                    hot_x,
                                      int                    hot_y )
{
     IDirectFBSurface_data *shape_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!shape)
          return DFB_INVARG;

     if (data->level != DLSCL_ADMINISTRATIVE)
          return DFB_ACCESSDENIED;

     shape_data = (IDirectFBSurface_data*)shape->priv;

     if (hot_x < 0  ||
         hot_y < 0  ||
         hot_x >= shape_data->surface->width  ||
         hot_y >= shape_data->surface->height)
          return DFB_INVARG;

     return dfb_windowstack_cursor_set_shape( data->stack,
                                              shape_data->surface,
                                              hot_x, hot_y );
}

static DFBResult
IDirectFBDisplayLayer_SetCursorOpacity( IDirectFBDisplayLayer *thiz,
                                        __u8                   opacity )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level != DLSCL_ADMINISTRATIVE)
          return DFB_ACCESSDENIED;

     return dfb_windowstack_cursor_set_opacity( data->stack, opacity );
}

static DFBResult
IDirectFBDisplayLayer_GetColorAdjustment( IDirectFBDisplayLayer *thiz,
                                          DFBColorAdjustment    *adj )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!adj)
          return DFB_INVARG;

     return dfb_layer_context_get_coloradjustment( data->context, adj );
}

static DFBResult
IDirectFBDisplayLayer_SetColorAdjustment( IDirectFBDisplayLayer *thiz,
                                          DFBColorAdjustment    *adj )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!adj || !adj->flags)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_coloradjustment( data->context, adj );
}

static DFBResult
IDirectFBDisplayLayer_WaitForSync( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     return dfb_layer_wait_vsync( data->layer );
}

DFBResult
IDirectFBDisplayLayer_Construct( IDirectFBDisplayLayer *thiz,
                                 CoreLayer             *layer )
{
     DFBResult         ret;
     CoreLayerContext *context;
     CoreLayerRegion  *region;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDisplayLayer)

     ret = dfb_layer_get_primary_context( layer, true, &context );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz )
          return ret;
     }

     ret = dfb_layer_context_get_primary_region( context, true, &region );
     if (ret) {
          dfb_layer_context_unref( context );
          DIRECT_DEALLOCATE_INTERFACE( thiz )
          return ret;
     }

     data->ref     = 1;
     data->screen  = dfb_layer_screen( layer );
     data->layer   = layer;
     data->context = context;
     data->region  = region;
     data->stack   = dfb_layer_context_windowstack( context );

     thiz->AddRef                = IDirectFBDisplayLayer_AddRef;
     thiz->Release               = IDirectFBDisplayLayer_Release;
     thiz->GetID                 = IDirectFBDisplayLayer_GetID;
     thiz->GetDescription        = IDirectFBDisplayLayer_GetDescription;
     thiz->GetSurface            = IDirectFBDisplayLayer_GetSurface;
     thiz->GetScreen             = IDirectFBDisplayLayer_GetScreen;
     thiz->SetCooperativeLevel   = IDirectFBDisplayLayer_SetCooperativeLevel;
     thiz->SetOpacity            = IDirectFBDisplayLayer_SetOpacity;
     thiz->GetCurrentOutputField = IDirectFBDisplayLayer_GetCurrentOutputField;
     thiz->SetSourceRectangle    = IDirectFBDisplayLayer_SetSourceRectangle;
     thiz->SetScreenLocation     = IDirectFBDisplayLayer_SetScreenLocation;
     thiz->SetSrcColorKey        = IDirectFBDisplayLayer_SetSrcColorKey;
     thiz->SetDstColorKey        = IDirectFBDisplayLayer_SetDstColorKey;
     thiz->GetLevel              = IDirectFBDisplayLayer_GetLevel;
     thiz->SetLevel              = IDirectFBDisplayLayer_SetLevel;
     thiz->GetConfiguration      = IDirectFBDisplayLayer_GetConfiguration;
     thiz->TestConfiguration     = IDirectFBDisplayLayer_TestConfiguration;
     thiz->SetConfiguration      = IDirectFBDisplayLayer_SetConfiguration;
     thiz->SetBackgroundMode     = IDirectFBDisplayLayer_SetBackgroundMode;
     thiz->SetBackgroundColor    = IDirectFBDisplayLayer_SetBackgroundColor;
     thiz->SetBackgroundImage    = IDirectFBDisplayLayer_SetBackgroundImage;
     thiz->GetColorAdjustment    = IDirectFBDisplayLayer_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBDisplayLayer_SetColorAdjustment;
     thiz->CreateWindow          = IDirectFBDisplayLayer_CreateWindow;
     thiz->GetWindow             = IDirectFBDisplayLayer_GetWindow;
     thiz->WarpCursor            = IDirectFBDisplayLayer_WarpCursor;
     thiz->SetCursorAcceleration = IDirectFBDisplayLayer_SetCursorAcceleration;
     thiz->EnableCursor          = IDirectFBDisplayLayer_EnableCursor;
     thiz->GetCursorPosition     = IDirectFBDisplayLayer_GetCursorPosition;
     thiz->SetCursorShape        = IDirectFBDisplayLayer_SetCursorShape;
     thiz->SetCursorOpacity      = IDirectFBDisplayLayer_SetCursorOpacity;
     thiz->SetFieldParity        = IDirectFBDisplayLayer_SetFieldParity;
     thiz->WaitForSync           = IDirectFBDisplayLayer_WaitForSync;

     return DFB_OK;
}

