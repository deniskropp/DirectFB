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

#include <endian.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

#include <core/fusion/shmalloc.h>
#include <core/fusion/arena.h>
#include <core/fusion/property.h>

#include "directfb.h"

#include "core.h"
#include "coredefs.h"
#include "coretypes.h"

#include "input.h"
#include "gfxcard.h"
#include "fbdev.h"
#include "layers.h"
#include "state.h"
#include "surfacemanager.h"
#include "windows.h"

#include "gfx/convert.h"
#include "gfx/util.h"
#include "misc/mem.h"
#include "misc/util.h"


#define CURSORFILE         DATADIR"/cursor.dat"

typedef struct {
     DFBDisplayLayerID        id;      /* unique id, functions as an index,
                                          primary layer has a fixed id */

     DisplayLayerInfo         layer_info;  
     void                    *layer_data;    

     /****/

     DFBDisplayLayerConfig    config;  /* current configuration */

     __u8                     opacity; /* if enabled this value controls
                                          blending of the whole layer */

     /* these are normalized values for stretching layers in hardware */
     struct {
          float     x, y;  /* 0,0 for the primary layer */
          float     w, h;  /* 1,1 for the primary layer */
     } screen;  

     DFBColorAdjustment       adjustment;      

     /****/

     int                      enabled; /* layers can be turned on and off */

     CoreWindowStack         *stack;   /* every layer has its own
                                          windowstack as every layer has
                                          its own pixel buffer */

     CoreSurface             *surface; /* surface of the layer */

     FusionProperty           lock;    /* purchased during exclusive access,
                                          leased during window stack repaint */

} DisplayLayerShared;  

struct _DisplayLayer {
     DisplayLayerShared *shared;

     GraphicsDevice     *device;

     void               *driver_data;
     void               *layer_data;   /* copy of shared->layer_data */

     DisplayLayerFuncs  *funcs;  
};  

typedef struct {
     int                 num;
     DisplayLayerShared *layers[MAX_LAYERS];
} CoreLayersField;

static CoreLayersField *layersfield = NULL;

static int           dfb_num_layers = 0;
static DisplayLayer *dfb_layers[MAX_LAYERS] = { NULL };


static DFBResult load_default_cursor ( DisplayLayer          *layer );

static DFBResult create_cursor_window( DisplayLayer          *layer,
                                       int                    width,
                                       int                    height );

static DFBResult allocate_surface    ( DisplayLayer          *layer );
static DFBResult reallocate_surface  ( DisplayLayer          *layer,
                                       DFBDisplayLayerConfig *config );
static DFBResult deallocate_surface  ( DisplayLayer          *layer );

static ReactionResult background_image_listener( const void *msg_data,
                                                 void       *ctx );

/** public **/

DFBResult
dfb_layers_initialize()
{
     DFB_ASSERT( layersfield == NULL );

     layersfield = shcalloc( 1, sizeof (CoreLayersField) );

#ifndef FUSION_FAKE
     arena_add_shared_field( dfb_core->arena, layersfield, "Core/Layers" );
#endif

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult
dfb_layers_join()
{
     int       i;
     DFBResult ret;

     DFB_ASSERT( layersfield == NULL );
     
     if (arena_get_shared_field( dfb_core->arena,
                                 (void**) &layersfield, "Core/Layers" ))
          return DFB_INIT;

     return DFB_OK;
}
#endif

DFBResult
dfb_layers_shutdown()
{
     int i;

     for (i=layersfield->num-1; i>=0; i--) {
          DisplayLayer *l = dfb_layers[i];

          if (l->shared->enabled)
               l->funcs->Disable( l, l->driver_data, l->layer_data );
          
          fusion_property_destroy( &l->shared->lock );

          DFBFREE( l );
     }

     shfree( layersfield );

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult
dfb_layers_leave()
{
     int i;

     for (i=0; i<layersfield->num; i++) {
          DFBFREE( dfb_layers[i] );
     }

     return DFB_OK;
}
#endif

#ifdef FUSION_FAKE
DFBResult
dfb_layers_suspend()
{
     int i;

     for (i=layersfield->num-1; i>=0; i--) {
          DisplayLayer *l = dfb_layers[i];

          if (l->shared->enabled)
               l->funcs->Disable( l, l->driver_data, l->layer_data );
     }

     return DFB_OK;
}

DFBResult
dfb_layers_resume()
{
     int i;

     for (i=0; i<layersfield->num; i++) {
          DisplayLayer *l = dfb_layers[i];

          if (l->shared->enabled) {
               l->funcs->Enable( l, l->driver_data, l->layer_data );
               
               l->funcs->SetConfiguration( l, &l->shared->config,
                                           l->driver_data, l->layer_data );

               if (l->shared->stack)
                    dfb_windowstack_repaint_all( l->shared->stack );
          }
     }

     return DFB_OK;
}
#endif

void
dfb_layers_register( GraphicsDevice    *device,
                     void              *driver_data,
                     DisplayLayerFuncs *funcs )
{
     DisplayLayer *layer;

     if (dfb_num_layers == MAX_LAYERS) {
          ERRORMSG( "DirectFB/Core/Layers: "
                    "Maximum number of layers reached!\n" );
          return;
     }

     /* allocate local data */
     layer = DFBCALLOC( 1, sizeof(DisplayLayer) );

     /* assign local pointers */
     layer->device      = device;
     layer->driver_data = driver_data;
     layer->funcs       = funcs;

     /* add it to the local list */
     dfb_layers[dfb_num_layers++] = layer;
}

DFBResult
dfb_layers_init_all()
{
     DFBResult ret;
     int       i;

     for (i=0; i<dfb_num_layers; i++) {
          DisplayLayer       *layer = dfb_layers[i];
          DisplayLayerShared *shared;

          /* allocate shared data */
          shared = shcalloc( 1, sizeof(DisplayLayerShared) );

          /* zero based counting */
          shared->id = i;
          
          /* init property for exclusive access and window stack repaints */
          fusion_property_init( &shared->lock );

          /* allocate shared layer data */
          shared->layer_data = shcalloc( 1, layer->funcs->LayerDataSize() );

          /* set default opacity */
          shared->opacity = 0xFF;

          /* set default screen location */
          shared->screen.x = 0.0f;
          shared->screen.y = 0.0f;
          shared->screen.w = 1.0f;
          shared->screen.h = 1.0f;

          /* initialize the layer gaining the default configuration,
             the default color adjustment and the layer information */
          ret = layer->funcs->InitLayer( layer->device, layer,
                                         &shared->layer_info,
                                         &shared->config,
                                         &shared->adjustment,
                                         layer->driver_data,
                                         shared->layer_data );
          if (ret) {
               fusion_property_destroy( &shared->lock );
               shfree( shared->layer_data );
               shfree( shared );
          }

          /* make a copy for faster access */
          layer->layer_data = shared->layer_data;
          
          /* store pointer to shared data */
          layer->shared = shared;
          
          /* add it to the shared list */
          layersfield->layers[ layersfield->num++ ] = shared;
     }

     /* enable the primary layer now */
     ret = dfb_layer_enable( dfb_layers[DLID_PRIMARY] );
     if (ret) {
          ERRORMSG("DirectFB/Core/layers: Failed to enable primary layer!\n");
          return ret;
     }

     return DFB_OK;
}

void dfb_layers_enumerate( DisplayLayerCallback  callback,
                           void                 *ctx )
{
     int i;

     for (i=0; i<layersfield->num; i++) {
          if (callback( dfb_layers[i], ctx ) == DFENUM_CANCEL)
               break;
     }
}

DisplayLayer *
dfb_layer_at( DFBDisplayLayerID id )
{
     DFB_ASSERT( id < layersfield->num);

     return dfb_layers[id];
}

/*
 * Lease layer during window stack repaints.
 */
DFBResult
dfb_layer_lease( DisplayLayer *layer )
{
     DFB_ASSERT( layer->shared->enabled );
     
     if (fusion_property_lease( &layer->shared->lock ))
          return DFB_LOCKED;

     return DFB_OK;
}

/*
 * Purchase layer for exclusive access.
 */
DFBResult
dfb_layer_purchase( DisplayLayer *layer )
{
     DFB_ASSERT( layer->shared->enabled );
     
     if (fusion_property_purchase( &layer->shared->lock ))
          return DFB_LOCKED;

     return DFB_OK;
}

/*
 * Release layer after lease/purchase.
 */
void
dfb_layer_release( DisplayLayer *layer, bool repaint )
{
     DFB_ASSERT( layer->shared->enabled );
     
     fusion_property_cede( &layer->shared->lock );
     
     if (repaint)
          dfb_windowstack_repaint_all( layer->shared->stack );
}


DFBResult
dfb_layer_enable( DisplayLayer *layer )
{
     DFBResult           ret;
     DisplayLayerShared *shared = layer->shared;
     
     if (shared->enabled)
          return DFB_OK;

     /* allocate the surface before enabling it */
     if (shared->layer_info.caps & DLCAPS_SURFACE) {
          ret = allocate_surface( layer );
          if (ret) {
               ERRORMSG("DirectFB/Core/layers: Could not allocate surface!\n");
               return ret;
          }
     }
     
     /* set default/last configuation, this shouldn't fail */
     ret = layer->funcs->SetConfiguration( layer, layer->driver_data,
                                           layer->layer_data, &shared->config );
     if (ret) {
          ERRORMSG("DirectFB/Core/layers: "
                   "Setting default/last configuration failed!\n");

          if (shared->surface)
               deallocate_surface( layer );

          return ret;
     }

     ret = layer->funcs->Enable( layer,
                                 layer->driver_data, layer->layer_data );
     if (ret) {
          if (shared->surface)
               deallocate_surface( layer );
          
          return ret;
     }

     shared->enabled = true;
     
     /* create a window stack on layers with a surface */
     if (shared->layer_info.caps & DLCAPS_SURFACE) {
          shared->stack = dfb_windowstack_new( layer,
                                               shared->config.width,
                                               shared->config.height );
     }
     
     return DFB_OK;
}

DFBResult
dfb_layer_disable( DisplayLayer *layer )
{
     DFBResult           ret;
     DisplayLayerShared *shared = layer->shared;
     
     if (!shared->enabled)
          return DFB_OK;

     ret = layer->funcs->Disable( layer,
                                  layer->driver_data, layer->layer_data );
     if (ret)
          return ret;

     shared->enabled = false;
     
     /* destroy the window stack if there is one */
     if (shared->stack) {
          dfb_windowstack_destroy( shared->stack );

          shared->stack = NULL;
     }
     
     /* deallocate the surface */
     if (shared->layer_info.caps & DLCAPS_SURFACE) {
          ret = deallocate_surface( layer );
          if (ret) {
               ERRORMSG("DirectFB/Core/layers: Surface deallocation failed!\n");
               return ret;
          }
     }
     
     return DFB_OK;
}


/*
 * configuration management
 */

DFBResult
dfb_layer_test_configuration( DisplayLayer               *layer,
                              DFBDisplayLayerConfig      *config,
                              DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags  unchanged = ~(config->flags);
     DisplayLayerShared         *shared    = layer->shared;

     /*
      * Fill all unchanged values with their current setting.
      */
     if (unchanged & DLCONF_BUFFERMODE)
          config->buffermode = shared->config.buffermode;

     if (unchanged & DLCONF_HEIGHT)
          config->height = shared->config.height;

     if (unchanged & DLCONF_OPTIONS)
          config->options = shared->config.options;

     if (unchanged & DLCONF_PIXELFORMAT)
          config->pixelformat = shared->config.pixelformat;

     if (unchanged & DLCONF_WIDTH)
          config->width = shared->config.width;

     /* call driver function now with a complete configuration */
     return layer->funcs->TestConfiguration( layer, layer->driver_data,
                                             layer->layer_data, config,
                                             failed );
}

DFBResult
dfb_layer_set_configuration( DisplayLayer          *layer,
                             DFBDisplayLayerConfig *config )
{
     DFBResult           ret;
     DisplayLayerShared *shared = layer->shared;

     DFB_ASSERT( shared->enabled );

     /* build new configuration and test it */
     ret = dfb_layer_test_configuration( layer, config, NULL );
     if (ret)
          return ret;

     /* reallocate the surface before setting the new configuration */
     if (shared->layer_info.caps & DLCAPS_SURFACE) {
          ret = reallocate_surface( layer, config );
          if (ret) {
               ERRORMSG("DirectFB/Core/layers: "
                        "Reallocation of layer surface failed!\n");
               return ret;
          }
     }
     
     /* apply new configuration, this shouldn't fail */
     ret = layer->funcs->SetConfiguration( layer, layer->driver_data,
                                           layer->layer_data, config );
     if (ret) {
          CAUTION("setting new configuration failed");
          return ret;
     }
     
     /*
      * Write back modified entries.
      */
     if (config->flags & DLCONF_BUFFERMODE)
          shared->config.buffermode = config->buffermode;

     if (config->flags & DLCONF_HEIGHT)
          shared->config.height = config->height;

     if (config->flags & DLCONF_OPTIONS)
          shared->config.options = config->options;

     if (config->flags & DLCONF_PIXELFORMAT)
          shared->config.pixelformat = config->pixelformat;

     if (config->flags & DLCONF_WIDTH)
          shared->config.width = config->width;

     /*
      * Update the valid region for the cursor.
      */
     shared->stack->cursor.region.x1 = 0;
     shared->stack->cursor.region.y1 = 0;
     shared->stack->cursor.region.x2 = config->width - 1;
     shared->stack->cursor.region.y2 = config->height - 1;

     return DFB_OK;
}

DFBResult
dfb_layer_get_configuration( DisplayLayer          *layer,
                             DFBDisplayLayerConfig *config )
{
     *config = layer->shared->config;

     return DFB_OK;
}


/*
 * background handling
 */

DFBResult
dfb_layer_set_background_mode ( DisplayLayer                  *layer,
                                DFBDisplayLayerBackgroundMode  mode )
{
     DisplayLayerShared *shared = layer->shared;
     CoreWindowStack    *stack  = shared->stack;

     DFB_ASSERT( shared->enabled );

     /* nothing to do if mode is the same */
     if (mode == stack->bg.mode)
          return DFB_OK;

     /* for these modes a surface is required */
     if ((mode == DLBM_IMAGE || mode == DLBM_TILE) && !stack->bg.image)
          return DFB_MISSINGIMAGE;

     /* set new mode */
     stack->bg.mode = mode;

     /* force an update of the window stack */
     if (mode != DLBM_DONTCARE)
          dfb_windowstack_repaint_all( stack );
     
     return DFB_OK;
}

DFBResult
dfb_layer_set_background_image( DisplayLayer *layer,
                                CoreSurface  *image )
{
     DisplayLayerShared *shared = layer->shared;
     CoreWindowStack    *stack  = shared->stack;

     DFB_ASSERT( shared->enabled );

     /* if the surface is changed */
     if (stack->bg.image != image) {
          /* detach listener from old surface */
          if (stack->bg.image)
               reactor_detach( stack->bg.image->reactor,
                               background_image_listener, layer );

          /* set new surface */
          stack->bg.image = image;

          /* attach listener to new surface */
          reactor_attach( image->reactor, background_image_listener, layer );
     }

     /* force an update of the window stack */
     if (stack->bg.mode == DLBM_IMAGE || stack->bg.mode == DLBM_TILE)
          dfb_windowstack_repaint_all( stack );
     
     return DFB_OK;
}

DFBResult
dfb_layer_set_background_color( DisplayLayer *layer,
                                DFBColor     *color )
{
     DisplayLayerShared *shared = layer->shared;
     CoreWindowStack    *stack  = shared->stack;

     DFB_ASSERT( shared->enabled );

     /* do nothing if color didn't change */
     if (dfb_colors_equal( &stack->bg.color, color ))
         return DFB_OK;
     
     /* set new color */
     stack->bg.color = *color;

     /* force an update of the window stack */
     if (stack->bg.mode == DLBM_COLOR)
          dfb_windowstack_repaint_all( stack );
     
     return DFB_OK;
}


/*
 * various functions
 */

CoreSurface *
dfb_layer_surface( const DisplayLayer *layer )
{
     DisplayLayerShared *shared = layer->shared;

     DFB_ASSERT( shared->surface );
     
     return shared->surface;
}

DFBDisplayLayerCapabilities
dfb_layer_capabilities( const DisplayLayer *layer )
{
     return layer->shared->layer_info.caps;
}

DFBDisplayLayerID
dfb_layer_id( const DisplayLayer *layer )
{
     return layer->shared->id;
}

DFBResult
dfb_layer_flip_buffers( DisplayLayer *layer )
{
     DisplayLayerShared *shared = layer->shared;

     DFB_ASSERT( shared->enabled );
     
     switch (shared->config.buffermode) {
          case DLBM_FRONTONLY:
               return DFB_UNSUPPORTED;

          case DLBM_BACKVIDEO:
               return layer->funcs->FlipBuffers( layer,
                                                 layer->driver_data,
                                                 layer->layer_data );
          
          case DLBM_BACKSYSTEM:
               dfb_back_to_front_copy( shared->surface, NULL );
               break;

          default:
               BUG("unknown buffer mode");
               return DFB_BUG;
     }

     return DFB_OK;
}

DFBResult
dfb_layer_create_window( DisplayLayer           *layer,
                         int                     x,
                         int                     y,
                         unsigned int            width,
                         unsigned int            height,
                         DFBWindowCapabilities   caps,
                         DFBSurfacePixelFormat   pixelformat,
                         CoreWindow            **window )
{
     DFBResult           ret;
     CoreWindow         *w;
     DisplayLayerShared *shared = layer->shared;

     DFB_ASSERT( shared->enabled );
     
     ret = dfb_window_create( shared->stack,
                              x, y, width, height, caps, pixelformat, &w );
     if (ret)
          return ret;

     *window = w;

     return DFB_OK;
}

DFBResult
dfb_layer_set_src_colorkey( DisplayLayer *layer,
                            __u8 r, __u8 g, __u8 b )
{
     __u32               key;
     DisplayLayerShared *shared = layer->shared;

     if (!layer->funcs->SetSrcColorKey)
          return DFB_UNSUPPORTED;

     if (shared->surface)
          key = color_to_pixel( shared->surface->format, r, g, b );
     else
          key = PIXEL_RGB32( r, g, b );

     return layer->funcs->SetSrcColorKey( layer, layer->driver_data,
                                          layer->layer_data, key );
}

DFBResult
dfb_layer_set_dst_colorkey( DisplayLayer *layer,
                            __u8 r, __u8 g, __u8 b )
{
     if (!layer->funcs->SetSrcColorKey)
          return DFB_UNSUPPORTED;
     
     return layer->funcs->SetDstColorKey( layer, layer->driver_data,
                                          layer->layer_data, r, g, b );
}

DFBResult
dfb_layer_set_screenlocation( DisplayLayer *layer,
                              float x, float y,
                              float width, float height )
{
     DFBResult           ret;
     DisplayLayerShared *shared = layer->shared;

     if (!layer->funcs->SetScreenLocation)
          return DFB_UNSUPPORTED;
     
     ret = layer->funcs->SetScreenLocation( layer, layer->driver_data,
                                            layer->layer_data,
                                            x, y, width, height );
     if (ret)
          return ret;

     shared->screen.x = x;
     shared->screen.y = x;
     shared->screen.w = width;
     shared->screen.h = height;

     return DFB_OK;
}

DFBResult
dfb_layer_set_opacity (DisplayLayer *layer, __u8 opacity)
{
     DFBResult           ret;
     DisplayLayerShared *shared = layer->shared;

     if (!layer->funcs->SetOpacity)
          return DFB_UNSUPPORTED;
     
     ret = layer->funcs->SetOpacity( layer, layer->driver_data,
                                     layer->layer_data, opacity );
     if (ret)
          return ret;

     shared->opacity = opacity;

     return DFB_OK;
}

DFBResult
dfb_layer_set_coloradjustment (DisplayLayer       *layer,
                               DFBColorAdjustment *adj)
{
     DFBResult                ret;
     DisplayLayerShared      *shared = layer->shared;
     DFBColorAdjustmentFlags  unchanged = ~adj->flags & shared->adjustment.flags;

     if (!layer->funcs->SetColorAdjustment)
          return DFB_UNSUPPORTED;

     /* if flags are set that are not in the default adjustment */
     if (adj->flags & ~shared->adjustment.flags)
          return DFB_UNSUPPORTED;
     
     /* fill unchanged values */
     if (unchanged & DCAF_BRIGHTNESS)
          adj->brightness = shared->adjustment.brightness;

     if (unchanged & DCAF_CONTRAST)
          adj->contrast = shared->adjustment.contrast;

     if (unchanged & DCAF_HUE)
          adj->hue = shared->adjustment.hue;

     if (unchanged & DCAF_SATURATION)
          adj->saturation = shared->adjustment.saturation;
     
     /* set new adjustment */
     ret = layer->funcs->SetColorAdjustment( layer, layer->driver_data,
                                             layer->layer_data, adj );
     if (ret)
          return ret;

     /* write back any changed values */
     if (adj->flags & DCAF_BRIGHTNESS)
          shared->adjustment.brightness = adj->brightness;

     if (adj->flags & DCAF_CONTRAST)
          shared->adjustment.contrast = adj->contrast;

     if (adj->flags & DCAF_HUE)
          shared->adjustment.hue = adj->hue;

     if (adj->flags & DCAF_SATURATION)
          shared->adjustment.saturation = adj->saturation;

     return DFB_OK;
}

DFBResult
dfb_layer_get_coloradjustment (DisplayLayer       *layer,
                               DFBColorAdjustment *adj)
{
     *adj = layer->shared->adjustment;

     return DFB_OK;
}

DFBResult
dfb_layer_get_cursor_position (DisplayLayer       *layer,
                               int                *x,
                               int                *y)
{
     DisplayLayerShared *shared = layer->shared;

     DFB_ASSERT( shared->enabled );

     if (x)
          *x = shared->stack->cursor.x;

     if (y)
          *y = shared->stack->cursor.y;

     return DFB_OK;
}

DFBSurfacePixelFormat
dfb_primary_layer_pixelformat()
{
     DisplayLayer *layer = dfb_layers[0];
     
     DFB_ASSERT( layer );
     DFB_ASSERT( layer->shared );
     
     return layer->shared->config.pixelformat;
}

void
dfb_primary_layer_rectangle( float x, float y,
                             float w, float h,
                             DFBRectangle *rect )
{
     DisplayLayer       *layer  = dfb_layers[0];
     DisplayLayerShared *shared;
     
     DFB_ASSERT( layer );
     DFB_ASSERT( layer->shared );

     shared = layer->shared;
     
     rect->x = (int)(x * (float)shared->config.width + 0.5f);
     rect->y = (int)(y * (float)shared->config.height + 0.5f);
     rect->w = (int)(w * (float)shared->config.width + 0.5f);
     rect->h = (int)(h * (float)shared->config.height + 0.5f);
}

/*
 * cursor control
 */

DFBResult
dfb_layer_cursor_enable( DisplayLayer *layer, int enable )
{
     DisplayLayerShared *shared = layer->shared;
     CoreWindowStack    *stack  = shared->stack;

     DFB_ASSERT( layer->shared->enabled );
     
     if (enable) {
          if (!stack->cursor.window) {
               DFBResult ret;

               ret = load_default_cursor( layer );
               if (ret)
                    return ret;
          }

          dfb_window_set_opacity( stack->cursor.window,
                                  stack->cursor.opacity );

          stack->cursor.enabled = 1;
     }
     else {
          if (stack->cursor.window)
               dfb_window_set_opacity( stack->cursor.window, 0 );

          stack->cursor.enabled = 0;
     }

     return DFB_OK;
}

DFBResult
dfb_layer_cursor_set_opacity( DisplayLayer *layer, __u8 opacity )
{
     CoreWindowStack *stack = layer->shared->stack;

     DFB_ASSERT( layer->shared->enabled );

     if (stack->cursor.enabled) {
          DFB_ASSERT( stack->cursor.window );

          dfb_window_set_opacity( stack->cursor.window, opacity );
     }

     stack->cursor.opacity = opacity;

     return DFB_OK;
}

DFBResult
dfb_layer_cursor_set_shape( DisplayLayer *layer,
                            CoreSurface  *shape,
                            int           hot_x,
                            int           hot_y )
{
     int                 dx, dy;
     DisplayLayerShared *shared = layer->shared;

     DFB_ASSERT( layer->shared->enabled );

     if (!shared->stack->cursor.window) {
          DFBResult ret =
          create_cursor_window( layer, shape->width, shape->height );

          if (ret)
               return ret;
     }
     else if (shared->stack->cursor.window->width != shape->width  ||
              shared->stack->cursor.window->height != shape->height) {
          dfb_window_resize( shared->stack->cursor.window,
                             shape->width, shape->height );
     }

     dfb_gfx_copy( shape, shared->stack->cursor.window->surface, NULL );

     dx = shared->stack->cursor.x - hot_x - shared->stack->cursor.window->x;
     dy = shared->stack->cursor.y - hot_y - shared->stack->cursor.window->y;

     if (dx || dy)
          dfb_window_move( shared->stack->cursor.window, dx, dy );
     else
          dfb_window_repaint( shared->stack->cursor.window, NULL );

     return DFB_OK;
}

DFBResult
dfb_layer_cursor_warp( DisplayLayer *layer, int x, int y )
{
     int              dx, dy;
     CoreWindowStack *stack = layer->shared->stack;

     DFB_ASSERT( layer->shared->enabled );

     dx = x - stack->cursor.x;
     dy = y - stack->cursor.y;

     dfb_windowstack_handle_motion( stack, dx, dy );

     return DFB_OK;
}

DFBResult
dfb_layer_cursor_set_acceleration( DisplayLayer *layer,
                                   int           numerator,
                                   int           denominator,
                                   int           threshold )
{
     CoreWindowStack *stack = layer->shared->stack;

     DFB_ASSERT( layer->shared->enabled );

     stack->cursor.numerator   = numerator;
     stack->cursor.denominator = denominator;
     stack->cursor.threshold   = threshold;
     
     return DFB_OK;
}


/** internal **/

/*
 * internal function that installs the cursor window
 * and fills it with data from 'cursor.dat'
 */
static DFBResult
load_default_cursor( DisplayLayer *layer )
{
     DFBResult           ret;
     int                 i;
     unsigned int        pitch;
     void               *data;
     FILE               *f;
     DisplayLayerShared *shared = layer->shared;
     CoreWindowStack    *stack  = shared->stack;

     if (!stack->cursor.window) {
          ret = create_cursor_window( layer, 40, 40 );
          if (ret)
               return ret;
     }

     /* open the file containing the cursors image data */
     f = fopen( CURSORFILE, "rb" );
     if (!f) {
          ret = errno2dfb( errno );
          PERRORMSG( "`" CURSORFILE "` could not be opened!\n" );
          return ret;
     }

     /* lock the surface of the window */
     ret = dfb_surface_soft_lock( stack->cursor.window->surface,
                                  DSLF_WRITE, &data, &pitch, 0 );
     if (ret) {
          ERRORMSG( "DirectFB/core/layers: "
                    "cannot lock the surface for cursor window data!\n" );
          fclose( f );

          return ret;
     }

     /* read from file directly into the cursor window surface */
     for (i=0; i<40; i++) {
          if (fread( data, 40*4, 1, f ) != 1) {
               ret = errno2dfb( errno );

               ERRORMSG( "DirectFB/core/layers: "
                         "unexpected end or read error of cursor data!\n" );

               dfb_surface_unlock( stack->cursor.window->surface, 0 );
               fclose( f );

               return ret;
          }
#if __BYTE_ORDER == __BIG_ENDIAN
          {
               int i = 40;
               __u32 *tmp_data = data;

               while (i--) {
                    *tmp_data = (*tmp_data & 0xFF000000) >> 24 |
                                (*tmp_data & 0x00FF0000) >>  8 |
                                (*tmp_data & 0x0000FF00) <<  8 |
                                (*tmp_data & 0x000000FF) << 24;
                    ++tmp_data;
               }
          }
#endif
          data += pitch;
     }

     fclose( f );
     dfb_surface_unlock( stack->cursor.window->surface, 0 );

     dfb_window_repaint( stack->cursor.window, NULL );

     return DFB_OK;
}

static DFBResult
create_cursor_window( DisplayLayer *layer,
                      int           width,
                      int           height )
{
     DFBResult           ret;
     CoreWindow         *cursor;
     DisplayLayerShared *shared = layer->shared;
     CoreWindowStack    *stack  = shared->stack;

     /* reinitialization check */
     if (stack->cursor.window) {
          BUG( "already created a cursor for this layer" );
          return DFB_BUG;
     }

     stack->cursor.opacity = 0xFF;
     stack->cursor.x = shared->config.width / 2;
     stack->cursor.y = shared->config.height / 2;

     /* create a super-top-most_event-and-focus-less window */
     ret = dfb_window_create( stack,
                              stack->cursor.x,
                              stack->cursor.y, width, height,
                              DWHC_TOPMOST | DWCAPS_ALPHACHANNEL,
                              DSPF_UNKNOWN, &cursor );
     if (ret) {
          ERRORMSG( "DirectFB/Core/layers: "
                    "failed creating a window for software cursor!\n" );
          return ret;
     }

     cursor->options = DWOP_GHOST;

     dfb_window_init( cursor );
     dfb_window_set_opacity( cursor, stack->cursor.opacity );

     stack->cursor.window  = cursor;

     return DFB_OK;
}


/*
 * layer surface (re/de)allocation
 */

static DFBResult
allocate_surface( DisplayLayer *layer )
{
     DisplayLayerShared *shared = layer->shared;
     
     DFB_ASSERT( shared->surface == NULL );

     if (layer->funcs->AllocateSurface)
          return layer->funcs->AllocateSurface( layer, layer->driver_data,
                                                layer->layer_data,
                                                &shared->config,
                                                &shared->surface );

     return dfb_surface_create( shared->config.width, shared->config.height,
                                shared->config.pixelformat, CSP_VIDEOONLY,
                                DSCAPS_VIDEOONLY, &shared->surface );
}

static DFBResult
reallocate_surface( DisplayLayer *layer, DFBDisplayLayerConfig *config )
{
     DFBResult           ret;
     DisplayLayerShared *shared = layer->shared;
     
     DFB_ASSERT( shared->surface != NULL );

     if (layer->funcs->ReallocateSurface)
          return layer->funcs->ReallocateSurface( layer, layer->driver_data,
                                                  layer->layer_data, config,
                                                  shared->surface );

     /* FIXME: implement buffer mode changes */
     if (shared->config.buffermode != config->buffermode) {
          ONCE("Changing the buffermode of layers is unimplemented!");
          return DFB_UNIMPLEMENTED;
     }

     /* FIXME: write surface management functions
               for easier configuration changes */
     ret = dfb_surface_reformat( shared->surface, config->width,
                                 config->height, config->pixelformat );
     if (ret)
          return ret;

     if (config->options & DLOP_INTERLACED_VIDEO)
          shared->surface->caps |= DSCAPS_INTERLACED;
     else
          shared->surface->caps &= ~DSCAPS_INTERLACED;

     return DFB_OK;
}

static DFBResult
deallocate_surface( DisplayLayer *layer )
{
     DisplayLayerShared *shared  = layer->shared;
     CoreSurface        *surface = shared->surface;

     DFB_ASSERT( surface != NULL );

     shared->surface = NULL;

     if (layer->funcs->DeallocateSurface)
          return layer->funcs->DeallocateSurface( layer, layer->driver_data,
                                                  layer->layer_data, surface );

     dfb_surface_destroy( surface );
     
     return DFB_OK;
}


/*
 * listen to the background image
 */
static ReactionResult
background_image_listener( const void *msg_data,
                           void       *ctx )
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*)msg_data;
     DisplayLayer            *layer        = (DisplayLayer*) ctx;
     CoreWindowStack         *stack        = layer->shared->stack;

     if (notification->flags & CSNF_DESTROY) {
          DEBUGMSG("DirectFB/core/layers: Surface for background vanished.\n");

          stack->bg.mode  = DLBM_COLOR;
          stack->bg.image = NULL;

          dfb_windowstack_repaint_all( stack );

          return RS_REMOVE;
     }

     if (notification->flags & (CSNF_FLIP | CSNF_SIZEFORMAT))
          dfb_windowstack_repaint_all( stack );

     return RS_OK;
}

