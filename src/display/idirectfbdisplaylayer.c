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

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include "idirectfbdisplaylayer.h"
#include "idirectfbscreen.h"
#include "idirectfbsurface.h"
#include "idirectfbsurface_layer.h"


D_DEBUG_DOMAIN( Layer, "IDirectFBDisplayLayer", "Display Layer Interface" );

/*
 * private data struct of IDirectFB
 */
typedef struct {
     int                              ref;              /* reference counter */
     DFBDisplayLayerDescription       desc;             /* description of the layer's caps */
     DFBDisplayLayerCooperativeLevel  level;            /* current cooperative level */
     CoreScreen                      *screen;           /* layer's screen */
     CoreLayer                       *layer;            /* core layer data */
     CoreLayerContext                *context;          /* shared or exclusive context */
     CoreLayerRegion                 *region;           /* primary region of the context */
     CoreWindowStack                 *stack;            /* stack of shared context */
     DFBBoolean                       switch_exclusive; /* switch to exclusive context after creation? */
     CoreDFB                         *core;
} IDirectFBDisplayLayer_data;



static void
IDirectFBDisplayLayer_Destruct( IDirectFBDisplayLayer *thiz )
{
     IDirectFBDisplayLayer_data *data = (IDirectFBDisplayLayer_data*)thiz->priv;

     D_DEBUG_AT( Layer, "IDirectFBDisplayLayer_Destruct()\n" );

     D_DEBUG_AT( Layer, "  -> unref region...\n" );

     dfb_layer_region_unref( data->region );

     D_DEBUG_AT( Layer, "  -> unref context...\n" );

     dfb_layer_context_unref( data->context );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     D_DEBUG_AT( Layer, "  -> done.\n" );
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

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!desc)
          return DFB_INVARG;

     *desc = data->desc;

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

     ret = IDirectFBSurface_Layer_Construct( surface, NULL, NULL, NULL,
                                             region, DSCAPS_NONE, data->core );

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
                    data->stack   = dfb_layer_context_windowstack( data->context );
               }

               break;

          case DLSCL_EXCLUSIVE:
               ret = dfb_layer_create_context( data->layer, &context );
               if (ret)
                    return ret;

               if (data->switch_exclusive) {
                    ret = dfb_layer_activate_context( data->layer, context );
                    if (ret) {
                         dfb_layer_context_unref( context );
                         return ret;
                    }
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
               data->stack   = dfb_layer_context_windowstack( data->context );

               break;

          default:
               return DFB_INVARG;
     }

     data->level = level;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_SetOpacity( IDirectFBDisplayLayer *thiz,
                                  u8                     opacity )
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
IDirectFBDisplayLayer_SetClipRegions( IDirectFBDisplayLayer *thiz,
                                      const DFBRegion       *regions,
                                      int                    num_regions,
                                      DFBBoolean             positive )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!regions || num_regions < 1)
          return DFB_INVARG;

     if (num_regions > data->desc.clip_regions)
          return DFB_UNSUPPORTED;

     if (data->level != DLSCL_EXCLUSIVE)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_clip_regions( data->context, regions, num_regions, positive );
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

     if (! D_FLAGS_IS_SET( data->desc.caps, DLCAPS_SCREEN_LOCATION ))
          return DFB_UNSUPPORTED;

     if (width <= 0 || height <= 0)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_screenlocation( data->context, &location );
}

static DFBResult
IDirectFBDisplayLayer_SetScreenPosition( IDirectFBDisplayLayer *thiz,
                                         int                    x,
                                         int                    y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (! D_FLAGS_IS_SET( data->desc.caps, DLCAPS_SCREEN_POSITION ))
          return DFB_UNSUPPORTED;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_screenposition( data->context, x, y );
}

static DFBResult
IDirectFBDisplayLayer_SetScreenRectangle( IDirectFBDisplayLayer *thiz,
                                          int                    x,
                                          int                    y,
                                          int                    width,
                                          int                    height )
{
     DFBRectangle rect = { x, y, width, height };

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (! D_FLAGS_IS_SET( data->desc.caps, DLCAPS_SCREEN_LOCATION ))
          return DFB_UNSUPPORTED;

     if (width <= 0 || height <= 0)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_screenrectangle( data->context, &rect );
}

static DFBResult
IDirectFBDisplayLayer_SetSrcColorKey( IDirectFBDisplayLayer *thiz,
                                      u8                     r,
                                      u8                     g,
                                      u8                     b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_src_colorkey( data->context, r, g, b, -1 );
}

static DFBResult
IDirectFBDisplayLayer_SetDstColorKey( IDirectFBDisplayLayer *thiz,
                                      u8                     r,
                                      u8                     g,
                                      u8                     b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_dst_colorkey( data->context, r, g, b, -1 );
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

     if (! D_FLAGS_IS_SET( data->desc.caps, DLCAPS_LEVELS ))
          return DFB_UNSUPPORTED;

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
IDirectFBDisplayLayer_TestConfiguration( IDirectFBDisplayLayer       *thiz,
                                         const DFBDisplayLayerConfig *config,
                                         DFBDisplayLayerConfigFlags  *failed )
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
IDirectFBDisplayLayer_SetConfiguration( IDirectFBDisplayLayer       *thiz,
                                        const DFBDisplayLayerConfig *config )
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

     if (data->level == DLSCL_SHARED)
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

     if (data->level == DLSCL_SHARED)
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
                                          u8 r, u8 g, u8 b, u8 a )
{
     DFBColor color = { a: a, r: r, g: g, b: b };

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_windowstack_set_background_color( data->stack, &color );
}

static DFBResult
IDirectFBDisplayLayer_CreateWindow( IDirectFBDisplayLayer       *thiz,
                                    const DFBWindowDescription  *desc,
                                    IDirectFBWindow            **window )
{
     CoreWindow           *w;
     DFBResult             ret;
     DFBWindowDescription  wd;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     memset( &wd, 0, sizeof(wd) );

     wd.flags = DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_POSX | DWDESC_POSY |
                DWDESC_PIXELFORMAT | DWDESC_SURFACE_CAPS | DWDESC_CAPS;

     wd.width  = (desc->flags & DWDESC_WIDTH)  ? desc->width  : 480;
     wd.height = (desc->flags & DWDESC_HEIGHT) ? desc->height : 300;
     wd.posx   = (desc->flags & DWDESC_POSX)   ? desc->posx   : 100;
     wd.posy   = (desc->flags & DWDESC_POSY)   ? desc->posy   : 100;

     if (desc->flags & DWDESC_CAPS)
          wd.caps = desc->caps;

     if (desc->flags & DWDESC_PIXELFORMAT)
          wd.pixelformat = desc->pixelformat;

     if (desc->flags & DWDESC_SURFACE_CAPS)
          wd.surface_caps = desc->surface_caps;

     if (desc->flags & DWDESC_PARENT) {
          wd.flags     |= DWDESC_PARENT;
          wd.parent_id  = desc->parent_id;
     }

     if (desc->flags & DWDESC_OPTIONS) {
          wd.flags   |= DWDESC_OPTIONS;
          wd.options  = desc->options;
     }

     if (desc->flags & DWDESC_STACKING) {
          wd.flags    |= DWDESC_STACKING;
          wd.stacking  = desc->stacking;
     }


     if ((wd.caps & ~DWCAPS_ALL) || !window)
          return DFB_INVARG;

     if (wd.width < 1 || wd.width > 4096 || wd.height < 1 || wd.height > 4096)
          return DFB_INVARG;

     ret = dfb_layer_context_create_window( data->core, data->context, &wd, &w );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( *window, IDirectFBWindow );

     return IDirectFBWindow_Construct( *window, w, data->layer, data->core );
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
   
      //remove for now
     //if (data->level == DLSCL_SHARED)
     //    return DFB_ACCESSDENIED;

     w = dfb_layer_context_find_window( data->context, id );
     if (!w)
          return DFB_IDNOTFOUND;

     DIRECT_ALLOCATE_INTERFACE( *window, IDirectFBWindow );

     return IDirectFBWindow_Construct( *window, w, data->layer, data->core );
}

static DFBResult
IDirectFBDisplayLayer_EnableCursor( IDirectFBDisplayLayer *thiz, int enable )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_windowstack_cursor_enable( data->core, data->stack, enable );
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

     if (data->level == DLSCL_SHARED)
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

     if (data->level == DLSCL_SHARED)
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

     if (data->level == DLSCL_SHARED)
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
                                        u8                     opacity )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == DLSCL_SHARED)
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
IDirectFBDisplayLayer_SetColorAdjustment( IDirectFBDisplayLayer    *thiz,
                                          const DFBColorAdjustment *adj )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!adj || (adj->flags & ~DCAF_ALL))
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     if (!adj->flags)
          return DFB_OK;

     return dfb_layer_context_set_coloradjustment( data->context, adj );
}

static DFBResult
IDirectFBDisplayLayer_WaitForSync( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     return dfb_layer_wait_vsync( data->layer );
}

static DFBResult
IDirectFBDisplayLayer_GetSourceDescriptions( IDirectFBDisplayLayer            *thiz,
                                             DFBDisplayLayerSourceDescription *ret_descriptions )
{
     int i;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!ret_descriptions)
          return DFB_INVARG;

     if (! D_FLAGS_IS_SET( data->desc.caps, DLCAPS_SOURCES ))
          return DFB_UNSUPPORTED;

     for (i=0; i<data->desc.sources; i++)
          dfb_layer_get_source_info( data->layer, i, &ret_descriptions[i] );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_SwitchContext( IDirectFBDisplayLayer *thiz,
                                     DFBBoolean             exclusive )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (!exclusive && data->level == DLSCL_EXCLUSIVE) {
          DFBResult         ret;
          CoreLayerContext *context;

          ret = dfb_layer_get_primary_context( data->layer, false, &context );
          if (ret)
               return ret;

          dfb_layer_activate_context( data->layer, context );

          dfb_layer_context_unref( context );
     }
     else
         dfb_layer_activate_context( data->layer, data->context );

     data->switch_exclusive = exclusive;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_SetRotation( IDirectFBDisplayLayer *thiz,
                                   int                    rotation )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer)

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return dfb_layer_context_set_rotation( data->context, rotation );
}

DFBResult
IDirectFBDisplayLayer_Construct( IDirectFBDisplayLayer *thiz,
                                 CoreLayer             *layer,
                                 CoreDFB               *core )
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

     data->ref              = 1;
     data->core             = core;
     data->screen           = dfb_layer_screen( layer );
     data->layer            = layer;
     data->context          = context;
     data->region           = region;
     data->stack            = dfb_layer_context_windowstack( context );
     data->switch_exclusive = DFB_TRUE;

     dfb_layer_get_description( data->layer, &data->desc );

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
     thiz->SetClipRegions        = IDirectFBDisplayLayer_SetClipRegions;
     thiz->WaitForSync           = IDirectFBDisplayLayer_WaitForSync;
     thiz->GetSourceDescriptions = IDirectFBDisplayLayer_GetSourceDescriptions;
     thiz->SetScreenPosition     = IDirectFBDisplayLayer_SetScreenPosition;
     thiz->SetScreenRectangle    = IDirectFBDisplayLayer_SetScreenRectangle;
     thiz->SwitchContext         = IDirectFBDisplayLayer_SwitchContext;
     thiz->SetRotation           = IDirectFBDisplayLayer_SetRotation;

     return DFB_OK;
}

