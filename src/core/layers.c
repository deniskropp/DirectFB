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

#include <endian.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

#include <core/fusion/shmalloc.h>
#include <core/fusion/arena.h>
#include <core/fusion/property.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/state.h>
#include <core/surfacemanager.h>
#include <core/windows.h>

#include <core/fbdev/fbdev.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/mem.h>
#include <misc/util.h>


#define CURSORFILE         DATADIR"/cursor.dat"

typedef struct {
     DFBDisplayLayerID        id;      /* unique id, functions as an index,
                                          primary layer has a fixed id */

     DisplayLayerInfo         layer_info;  
     void                    *layer_data;    

     /****/

     DFBDisplayLayerConfig    config;  /* current configuration */
     
     DFBDisplayLayerConfig    last_config;  /* last 'shared' configuration */

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

     bool                     exclusive; /* helps to detect dead excl. access */
} DisplayLayerShared;  

struct _DisplayLayer {
     DisplayLayerShared *shared;

     GraphicsDevice     *device;

     void               *driver_data;
     void               *layer_data;   /* copy of shared->layer_data */

     DisplayLayerFuncs  *funcs;

     CardState           state;
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

static ReactionResult layer_surface_listener   ( const void *msg_data,
                                                 void       *ctx );
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
     DFB_ASSERT( layersfield == NULL );
     
     if (arena_get_shared_field( dfb_core->arena,
                                 (void**) &layersfield, "Core/Layers" ))
          return DFB_INIT;

     return DFB_OK;
}
#endif

DFBResult
dfb_layers_shutdown( bool emergency )
{
     int i;

     if (!layersfield)
          return DFB_OK;

     /* Begin with the most recently added */
     for (i=layersfield->num-1; i>=0; i--) {
          DisplayLayer *l = dfb_layers[i];

          if (emergency && l->shared->enabled) {
               /* Just turn it off during emergency shutdown */
               l->funcs->Disable( l, l->driver_data, l->layer_data );
          }
          else {
               /* Disable layer, destroy surface and
                  window stack (including windows and their surfaces) */
               dfb_layer_disable( l );
          }
          
          /* Destroy property */
          fusion_property_destroy( &l->shared->lock );

          /* Free shared layer driver data */
          if (l->shared->layer_data)
               shfree( l->shared->layer_data );

          /* Free shared layer data */
          shfree( l->shared );

          /* Deinit state for stack repaints. */
          dfb_state_set_destination( &l->state, NULL );
          dfb_state_destroy( &l->state );
          
          /* Free local layer data */
          DFBFREE( l );
     }

     /* Free shared layer field */
     shfree( layersfield );

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult
dfb_layers_leave( bool emergency )
{
     int i;

     /* Free all local data */
     for (i=0; i<layersfield->num; i++) {
          DisplayLayer *layer = dfb_layers[i];

          /* Deinit state for stack repaints. */
          dfb_state_set_destination( &layer->state, NULL );
          dfb_state_destroy( &layer->state );

          /* Free local layer data */
          DFBFREE( layer );
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
               
               l->funcs->SetConfiguration( l, l->driver_data,
                                           l->layer_data, &l->shared->config );

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

     /* Initialize the state for window stack repaints */
     dfb_state_init( &layer->state );

     /* add it to the local list */
     dfb_layers[dfb_num_layers++] = layer;
}

typedef void (*AnyFunc)();

void
dfb_layers_hook_primary( GraphicsDevice     *device,
                         void               *driver_data,
                         DisplayLayerFuncs  *funcs,
                         DisplayLayerFuncs  *primary_funcs,
                         void              **primary_driver_data )
{
     int           i;
     int           entries;
     DisplayLayer *primary = dfb_layers[0];

     DFB_ASSERT( primary != NULL );
     DFB_ASSERT( funcs != NULL );

     /* copy content of original function table */
     if (primary_funcs)
          memcpy( primary_funcs, primary->funcs, sizeof(DisplayLayerFuncs) );

     /* copy pointer to original driver data */
     if (primary_driver_data)
          *primary_driver_data = primary->driver_data;
     
     /* replace all entries in the old table that aren't NULL in the new one */
     entries = sizeof(DisplayLayerFuncs) / sizeof(void(*)());
     for (i=0; i<entries; i++) {
          AnyFunc *newfuncs = (AnyFunc*) funcs;
          AnyFunc *oldfuncs = (AnyFunc*) primary->funcs;

          if (newfuncs[i])
               oldfuncs[i] = newfuncs[i];
     }

     /* replace device and driver data pointer */
     primary->device      = device;
     primary->driver_data = driver_data;
}

void
dfb_layers_replace_primary( GraphicsDevice     *device,
                            void               *driver_data,
                            DisplayLayerFuncs  *funcs )
{
     DisplayLayer *primary = dfb_layers[0];

     DFB_ASSERT( primary != NULL );
     DFB_ASSERT( funcs != NULL );

     /* replace device, function table and driver data pointer */
     primary->device      = device;
     primary->funcs       = funcs;
     primary->driver_data = driver_data;
}

DFBResult
dfb_layers_init_all()
{
     DFBResult ret;
     int       i;

     for (i=0; i<dfb_num_layers; i++) {
          int                 layer_data_size;
          DisplayLayerShared *shared;
          DisplayLayer       *layer = dfb_layers[i];
          
          /* allocate shared data */
          shared = shcalloc( 1, sizeof(DisplayLayerShared) );

          /* zero based counting */
          shared->id = i;
          
          /* init property for exclusive access and window stack repaints */
          fusion_property_init( &shared->lock );

          /* allocate shared layer driver data */
          layer_data_size = layer->funcs->LayerDataSize();
          if (layer_data_size > 0)
               shared->layer_data = shcalloc( 1, layer_data_size );

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

#ifndef FUSION_FAKE
DFBResult
dfb_layers_join_all()
{
     int i;

     if (dfb_num_layers != layersfield->num)
          CAUTION("Number of layers does not match!");

     for (i=0; i<dfb_num_layers; i++) {
          DisplayLayer       *layer  = dfb_layers[i];
          DisplayLayerShared *shared = layersfield->layers[i];

          /* make a copy for faster access */
          layer->layer_data = shared->layer_data;
          
          /* store pointer to shared data */
          layer->shared = shared;
     }

     return DFB_OK;
}
#endif

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

     /* This can only be true if process with exclusive access died. */
     if (layer->shared->exclusive) {
          /* Restore the last configuration for shared access. */
          dfb_layer_set_configuration( layer, &layer->shared->last_config );

          /* Clear exclusive access. */
          layer->shared->exclusive = false;
     }
     
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

     /* Backup configuration of shared access. */
     if (!layer->shared->exclusive)
          layer->shared->last_config = layer->shared->config;
     
     /* Indicate exclusive access. */
     layer->shared->exclusive = true;

     return DFB_OK;
}

/*
 * Release layer after lease/purchase.
 */
void
dfb_layer_release( DisplayLayer *layer, bool repaint )
{
     DFB_ASSERT( layer->shared->enabled );
     
     /* If returning from exclusive access... */
     if (layer->shared->exclusive) {
          /* Restore the last configuration for shared access. */
          dfb_layer_set_configuration( layer, &layer->shared->last_config );

          /* Clear exclusive access. */
          layer->shared->exclusive = false;
     }
     
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
     if (shared->layer_info.desc.caps & DLCAPS_SURFACE) {
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

     /* enable the display layer */
     ret = layer->funcs->Enable( layer,
                                 layer->driver_data, layer->layer_data );
     if (ret) {
          if (shared->surface)
               deallocate_surface( layer );
          
          return ret;
     }

     shared->enabled = true;

     if (shared->surface) {
          CoreSurface *surface = shared->surface;

          /* attach surface listener for palette and field switches */
          dfb_surface_attach( surface, layer_surface_listener, layer );

          /* set default palette */
          if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ) &&
              surface->palette && layer->funcs->SetPalette)
          {
               layer->funcs->SetPalette( layer, layer->driver_data,
                                         layer->layer_data, surface->palette );
          }
          
          /* create a window stack on layers with a surface */
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

     /* call driver's disable function */
     ret = layer->funcs->Disable( layer,
                                  layer->driver_data, layer->layer_data );
     if (ret && ret != DFB_UNSUPPORTED)
          return ret;

     if (shared->surface)
          dfb_surface_detach( shared->surface, layer_surface_listener, layer );
     
     /* destroy the window stack if there is one */
     if (shared->stack) {
          CoreWindowStack *stack = shared->stack;
          
          dfb_windowstack_destroy( stack );

          shared->stack = NULL;
          
          /* detach listener from background surface and unlink it */
          if (stack->bg.image) {
               dfb_surface_detach( stack->bg.image,
                                   background_image_listener, layer );
               dfb_surface_unlink( stack->bg.image );
          }
     }
     
     /* deallocate the surface */
     if (shared->surface) {
          ret = deallocate_surface( layer );
          if (ret) {
               ERRORMSG("DirectFB/Core/layers: Surface deallocation failed!\n");
               return ret;
          }
     }
     
     shared->enabled = false;
     
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
     if (shared->layer_info.desc.caps & DLCAPS_SURFACE) {
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
     
     if (shared->layer_info.desc.caps & DLCAPS_SURFACE) {
          CoreSurface *surface = shared->surface;

          /* reset palette */
          if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ) &&
              surface->palette && layer->funcs->SetPalette)
          {
               layer->funcs->SetPalette( layer, layer->driver_data,
                                         layer->layer_data, surface->palette );
          }
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
      * Tell the windowing core about the new size.
      */
     if (shared->stack)
          dfb_windowstack_resize( shared->stack,
                                  config->width, config->height );

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
          /* detach listener from old surface and unlink it */
          if (stack->bg.image) {
               dfb_surface_detach( stack->bg.image,
                                   background_image_listener, layer );
               dfb_surface_unlink( stack->bg.image );
          }

          /* link surface object */
          dfb_surface_link( &stack->bg.image, image );
          
          /* attach listener to new surface */
          dfb_surface_attach( image, background_image_listener, layer );
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

CardState *
dfb_layer_state( DisplayLayer *layer )
{
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->surface != NULL );

     dfb_state_set_destination( &layer->state, layer->shared->surface );
     
     return &layer->state;
}

void
dfb_layer_description( const DisplayLayer         *layer,
                       DFBDisplayLayerDescription *desc )
{
     *desc = layer->shared->layer_info.desc;
}

DFBDisplayLayerID
dfb_layer_id( const DisplayLayer *layer )
{
     return layer->shared->id;
}

DFBResult
dfb_layer_flip_buffers( DisplayLayer *layer, DFBSurfaceFlipFlags flags )
{
     DisplayLayerShared *shared = layer->shared;

     DFB_ASSERT( shared->enabled );
     
     switch (shared->config.buffermode) {
          case DLBM_FRONTONLY:
               return DFB_UNSUPPORTED;

          case DLBM_BACKVIDEO:
               return layer->funcs->FlipBuffers( layer,
                                                 layer->driver_data,
                                                 layer->layer_data, flags );
          
          case DLBM_BACKSYSTEM:
               if (flags & DSFLIP_WAITFORSYNC)
                    dfb_fbdev_wait_vsync();
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
                         int                     width,
                         int                     height,
                         DFBWindowCapabilities   caps,
                         DFBSurfaceCapabilities  surface_caps,
                         DFBSurfacePixelFormat   pixelformat,
                         CoreWindow            **window )
{
     DFBResult           ret;
     CoreWindow         *w;
     DisplayLayerShared *shared = layer->shared;

     DFB_ASSERT( shared->enabled );
     
     ret = dfb_window_create( shared->stack, x, y, width, height,
                              caps, surface_caps, pixelformat, &w );
     if (ret)
          return ret;

     *window = w;

     return DFB_OK;
}

CoreWindow *dfb_layer_find_window( DisplayLayer *layer, DFBWindowID id )
{
     int                  i;
     DisplayLayerShared  *shared  = layer->shared;
     CoreWindowStack     *stack   = shared->stack;
     int                  num     = stack->num_windows;
     CoreWindow         **windows = stack->windows;

     /* FIXME: make thread safe, add assertions */

     for (i=0; i<num; i++)
          if (windows[i]->id == id)
               return windows[i];
     
     return NULL;
}

DFBResult
dfb_layer_set_src_colorkey( DisplayLayer *layer,
                            __u8 r, __u8 g, __u8 b )
{
     if (!layer->funcs->SetSrcColorKey)
          return DFB_UNSUPPORTED;

     return layer->funcs->SetSrcColorKey( layer, layer->driver_data,
                                          layer->layer_data, r, g, b );
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
dfb_layer_get_level( DisplayLayer *layer, int *level )
{
     if (!layer->funcs->GetLevel)
          return DFB_UNSUPPORTED;
     
     return layer->funcs->GetLevel( layer, layer->driver_data,
                                    layer->layer_data, level );
}

DFBResult
dfb_layer_set_level( DisplayLayer *layer, int level )
{
     if (!layer->funcs->SetLevel)
          return DFB_UNSUPPORTED;
     
     return layer->funcs->SetLevel( layer, layer->driver_data,
                                    layer->layer_data, level );
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
          dfb_window_repaint( shared->stack->cursor.window, NULL, 0 );

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

     dfb_window_repaint( stack->cursor.window, NULL, 0 );

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
                              DSCAPS_NONE, DSPF_UNKNOWN, &cursor );
     if (ret) {
          ERRORMSG( "DirectFB/Core/layers: "
                    "Failed creating a window for software cursor!\n" );
          return ret;
     }

     cursor->events   = 0;
     cursor->options |= DWOP_GHOST;

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
     DFBSurfaceCapabilities  caps   = DSCAPS_VIDEOONLY;
     DisplayLayerShared     *shared = layer->shared;
     
     DFB_ASSERT( shared->surface == NULL );

     if (layer->funcs->AllocateSurface)
          return layer->funcs->AllocateSurface( layer, layer->driver_data,
                                                layer->layer_data,
                                                &shared->config,
                                                &shared->surface );

     /* choose buffermode */
     if (shared->config.flags & DLCONF_BUFFERMODE) {
          switch (shared->config.buffermode) {
               case DLBM_FRONTONLY:
                    break;

               case DLBM_BACKVIDEO:
                    caps |= DSCAPS_FLIPPING;
                    break;

               case DLBM_BACKSYSTEM:
                    ONCE("DLBM_BACKSYSTEM in default config is unimplemented");
                    break;

               default:
                    BUG("unknown buffermode");
                    break;
          }
     }

     return dfb_surface_create( shared->config.width, shared->config.height,
                                shared->config.pixelformat, CSP_VIDEOONLY,
                                caps, NULL, &shared->surface );
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

     /* FIXME: write surface management functions
               for easier configuration changes */
     
     if (shared->config.buffermode != config->buffermode) {
          switch (config->buffermode) {
               case DLBM_BACKVIDEO:
                    shared->surface->caps |= DSCAPS_FLIPPING;
                    ret = dfb_surface_reconfig( shared->surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;
               case DLBM_BACKSYSTEM:
                    shared->surface->caps |= DSCAPS_FLIPPING;
                    ret = dfb_surface_reconfig( shared->surface,
                                                CSP_VIDEOONLY, CSP_SYSTEMONLY );
                    break;
               case DLBM_FRONTONLY:
                    shared->surface->caps &= ~DSCAPS_FLIPPING;
                    ret = dfb_surface_reconfig( shared->surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;
               
               default:
                    BUG("unknown buffermode");
                    return DFB_BUG;
          }
          
          if (ret)
               return ret;
     }

     ret = dfb_surface_reformat( shared->surface, config->width,
                                 config->height, config->pixelformat );
     if (ret)
          return ret;

     if (config->options & DLOP_DEINTERLACING)
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

     dfb_surface_unref( surface );
     
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

     if (!stack)
          return RS_REMOVE;

     if (notification->flags & CSNF_DESTROY) {
          ERRORMSG("DirectFB/core/layers: Surface for background vanished.\n");

          stack->bg.mode  = DLBM_COLOR;
          stack->bg.image = NULL;

          dfb_windowstack_repaint_all( stack );

          return RS_REMOVE;
     }

     if (notification->flags & (CSNF_FLIP | CSNF_SIZEFORMAT))
          dfb_windowstack_repaint_all( stack );

     return RS_OK;
}

/*
 * listen to the layer's surface
 */
static ReactionResult
layer_surface_listener( const void *msg_data, void *ctx )
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*)msg_data;
     DisplayLayer            *layer        = (DisplayLayer*) ctx;
     CoreSurface             *surface      = notification->surface;
     DisplayLayerFuncs       *funcs        = layer->funcs;
     CoreSurfaceNotificationFlags flags    = notification->flags;

     if (notification->flags & CSNF_DESTROY)
          return RS_REMOVE;

     if ((flags & CSNF_PALETTE) && surface->palette && funcs->SetPalette)
          funcs->SetPalette( layer, layer->driver_data,
                             layer->layer_data, surface->palette );

     if ((flags & (CSNF_SET_EVEN | CSNF_SET_ODD)) && funcs->SetField)
          funcs->SetField( layer, layer->driver_data,
                           layer->layer_data, (flags & CSNF_SET_ODD) ? 1 : 0 );

     return RS_OK;
}

