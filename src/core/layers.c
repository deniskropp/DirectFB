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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <core/fusion/shmalloc.h>
#include <core/fusion/arena.h>
#include <core/fusion/property.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core_parts.h>

#include <core/input.h>
#include <core/gfxcard.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/state.h>
#include <core/palette.h>
#include <core/system.h>
#include <core/surfacemanager.h>
#include <core/windows.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/mem.h>
#include <misc/memcpy.h>
#include <misc/util.h>

#include <core/layers_internal.h>


#define CURSORFILE         DATADIR"/cursor.dat"


typedef struct {
     int                 num;
     DisplayLayerShared *layers[MAX_LAYERS];
} CoreLayersField;

static CoreLayersField *layersfield = NULL;

static int           dfb_num_layers = 0;
static DisplayLayer *dfb_layers[MAX_LAYERS] = { NULL };


DFB_CORE_PART( layers, 0, sizeof(CoreLayersField) )


static DFBResult load_default_cursor ( DisplayLayer          *layer );

static DFBResult create_cursor_window( DisplayLayer          *layer,
                                       int                    width,
                                       int                    height );

static DFBResult allocate_surface    ( DisplayLayer          *layer,
                                       CoreLayerRegion       *region,
                                       DFBDisplayLayerConfig *config );
static DFBResult reallocate_surface  ( DisplayLayer          *layer,
                                       CoreLayerRegion       *region,
                                       DFBDisplayLayerConfig *config );
static DFBResult deallocate_surface  ( DisplayLayer          *layer,
                                       CoreLayerRegion       *region );


/** public **/

static DFBResult
dfb_layers_initialize( void *data_local, void *data_shared )
{
     int       i;
     DFBResult ret;

     DFB_ASSERT( layersfield == NULL );
     DFB_ASSERT( data_shared != NULL );

     layersfield = data_shared;

     for (i=0; i<dfb_num_layers; i++) {
          int                 layer_data_size;
          DisplayLayerShared *shared;
          DisplayLayer       *layer = dfb_layers[i];

          /* allocate shared data */
          shared = SHCALLOC( 1, sizeof(DisplayLayerShared) );

          /* zero based counting */
          shared->id = i;

          /* init property for exclusive access and window stack repaints */
          fusion_property_init( &shared->lock );

          /* allocate shared layer driver data */
          layer_data_size = layer->funcs->LayerDataSize();
          if (layer_data_size > 0)
               shared->layer_data = SHCALLOC( 1, layer_data_size );

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
               ERRORMSG("DirectFB/Core/layers: "
                        "Failed to initialize layer %d!\n", shared->id);

               fusion_property_destroy( &shared->lock );

               if (shared->layer_data)
                    SHFREE( shared->layer_data );

               SHFREE( shared );

               return ret;
          }

          /* create the object pool for the regions */
          shared->region_pool = dfb_layer_region_pool_create( shared->id );

          /* initialize the vector for the regions */
          fusion_vector_init( &shared->regions, 4 );

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

static DFBResult
dfb_layers_join( void *data_local, void *data_shared )
{
     int i;

     DFB_ASSERT( layersfield == NULL );
     DFB_ASSERT( data_shared != NULL );

     layersfield = data_shared;

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

static DFBResult
dfb_layers_shutdown( bool emergency )
{
     int i;

     DFB_ASSERT( layersfield != NULL );

     /* Begin with the most recently added */
     for (i=layersfield->num-1; i>=0; i--) {
          DisplayLayer       *l      = dfb_layers[i];
          DisplayLayerShared *shared = l->shared;

#if 0 /* FIXME: always need to detach global reactions */
          if (emergency && l->shared->enabled) {
               /* Just turn it off during emergency shutdown */
               l->funcs->Disable( l, l->driver_data, l->layer_data );
          }
          else {
#endif
               /* Disable layer, destroy surface and
                  window stack (including windows and their surfaces) */
               dfb_layer_disable( l );
#if 0
          }
#endif

          /* Destroy property */
          fusion_property_destroy( &l->shared->lock );

          /* Free shared layer driver data */
          if (l->shared->layer_data)
               SHFREE( l->shared->layer_data );

          /* Deinit state for stack repaints. */
          dfb_state_set_destination( &l->state, NULL );
          dfb_state_destroy( &l->state );

          /* Destroy the object pool for the regions. */
          fusion_object_pool_destroy( shared->region_pool );

          /* Destroy the vector for the regions. */
          fusion_vector_destroy( &shared->regions );

          /* Free shared layer data */
          SHFREE( l->shared );

          /* Free local layer data */
          DFBFREE( l );
     }

     layersfield = NULL;

     dfb_num_layers = 0;

     return DFB_OK;
}

static DFBResult
dfb_layers_leave( bool emergency )
{
     int i;

     DFB_ASSERT( layersfield != NULL );

     /* Free all local data */
     for (i=0; i<layersfield->num; i++) {
          DisplayLayer *layer = dfb_layers[i];

          /* Deinit state for stack repaints. */
          dfb_state_set_destination( &layer->state, NULL );
          dfb_state_destroy( &layer->state );

          /* Free local layer data */
          DFBFREE( layer );
     }

     layersfield = NULL;

     dfb_num_layers = 0;

     return DFB_OK;
}

static DFBResult
dfb_layers_suspend()
{
     int i;

     DFB_ASSERT( layersfield != NULL );

     DEBUGMSG( "DirectFB/core/layers: suspending...\n" );

     for (i=layersfield->num-1; i>=0; i--) {
          if (fusion_property_purchase( &dfb_layers[i]->shared->lock )) {
               int n;

               for (n=i+1; n<layersfield->num; n++)
                    fusion_property_cede( &dfb_layers[n]->shared->lock );

               return DFB_LOCKED;
          }
     }

     for (i=layersfield->num-1; i>=0; i--) {
          DisplayLayer *layer = dfb_layers[i];

          /* Flush pressed keys. */
          if (layer->shared->stack)
               dfb_windowstack_flush_keys( layer->shared->stack );

          if (layer->shared->enabled)
               layer->funcs->Disable( layer,
                                      layer->driver_data, layer->layer_data );

          layer->shared->exclusive = true;
     }

     DEBUGMSG( "DirectFB/core/layers: suspended.\n" );

     return DFB_OK;
}

static DFBResult
dfb_layers_resume()
{
     int i;

     DFB_ASSERT( layersfield != NULL );

     DEBUGMSG( "DirectFB/core/layers: resuming...\n" );

     for (i=0; i<layersfield->num; i++) {
          DisplayLayer *layer = dfb_layers[i];

          if (layer->shared->enabled) {
               layer->funcs->Enable( layer,
                                     layer->driver_data, layer->layer_data );

               layer->funcs->SetConfiguration( layer, layer->driver_data,
                                               layer->layer_data,
                                               &layer->shared->config );

               layer->funcs->SetOpacity( layer,
                                         layer->driver_data, layer->layer_data,
                                         layer->shared->opacity );
          }

          fusion_property_cede( &layer->shared->lock );

          if (layer->shared->stack)
               dfb_windowstack_repaint_all( layer->shared->stack );

          layer->shared->exclusive = false;
     }

     DEBUGMSG( "DirectFB/core/layers: resumed.\n" );

     return DFB_OK;
}

void
dfb_layers_register( GraphicsDevice    *device,
                     void              *driver_data,
                     DisplayLayerFuncs *funcs )
{
     DisplayLayer *layer;

     DFB_ASSERT( funcs != NULL );

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
          dfb_memcpy( primary_funcs, primary->funcs, sizeof(DisplayLayerFuncs) );

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

void
dfb_layers_enumerate( DisplayLayerCallback  callback,
                      void                 *ctx )
{
     int i;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( callback != NULL );

     for (i=0; i<layersfield->num; i++) {
          if (callback( dfb_layers[i], ctx ) == DFENUM_CANCEL)
               break;
     }
}

DisplayLayer *
dfb_layer_at( DFBDisplayLayerID id )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( id >= 0);
     DFB_ASSERT( id < layersfield->num);

     return dfb_layers[id];
}

DisplayLayer *
dfb_layer_at_translated( DFBDisplayLayerID id )
{
     DFB_ASSERT( layersfield != NULL );

     if (dfb_config->primary_layer > 0 &&
         dfb_config->primary_layer < layersfield->num)
     {
          if (id == DLID_PRIMARY)
               return dfb_layer_at( dfb_config->primary_layer );

          if (id == dfb_config->primary_layer)
               return dfb_layer_at( DLID_PRIMARY );
     }

     return dfb_layer_at( id );
}

/*
 * Lease layer during window stack repaints.
 */
DFBResult
dfb_layer_lease( DisplayLayer *layer )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     if (fusion_property_lease( &layer->shared->lock ))
          return DFB_LOCKED;

     /* This can only be true if process with exclusive access died. */
     if (layer->shared->exclusive) {
          /* Restore the last configuration for shared access. */
          dfb_layer_set_configuration( layer, &layer->shared->last_config );

          /* Clear exclusive access. */
          layer->shared->exclusive = false;

          if (layer->shared->stack) {
               fusion_property_cede( &layer->shared->lock );

               dfb_windowstack_repaint_all( layer->shared->stack );

               return dfb_layer_lease( layer );
          }
     }

     return DFB_OK;
}

/*
 * Purchase layer for exclusive access.
 */
DFBResult
dfb_layer_purchase( DisplayLayer *layer )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     if (fusion_property_purchase( &layer->shared->lock ))
          return DFB_LOCKED;

     /* Flush pressed keys and
        synchronize the content of the window stack's front and back buffer. */
     if (layer->shared->stack) {
          dfb_windowstack_flush_keys( layer->shared->stack );
          dfb_windowstack_sync_buffers( layer->shared->stack );
     }

     /* Indicate exclusive access. */
     layer->shared->exclusive = true;

     return DFB_OK;
}

/*
 * Kill the process that purchased the layer.
 */
DFBResult
dfb_layer_holdup( DisplayLayer *layer )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     if (layer->shared->exclusive)
          if (fusion_property_holdup( &layer->shared->lock ))
               return DFB_FUSION;

     return DFB_OK;
}

/*
 * Release layer after lease/purchase.
 */
void
dfb_layer_release( DisplayLayer *layer, bool repaint )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     /* If returning from exclusive access... */
     if (layer->shared->exclusive) {
          /* Restore the last configuration for shared access. */
          dfb_layer_set_configuration( layer, &layer->shared->last_config );

          /* Clear exclusive access. */
          layer->shared->exclusive = false;
     }

     fusion_property_cede( &layer->shared->lock );

     if (repaint && layer->shared->stack)
          dfb_windowstack_repaint_all( layer->shared->stack );
}


DFBResult
dfb_layer_enable( DisplayLayer *layer )
{
     DFBResult           ret;
     DisplayLayerShared *shared;
     DisplayLayerFuncs  *funcs;
     CoreLayerRegion    *region;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );

     shared = layer->shared;
     funcs  = layer->funcs;

     /* FIXME: add reference counting */
     if (shared->enabled) {
          DFB_ASSERT( shared->regions.num_elements > 0 );

          return dfb_layer_region_ref( shared->regions.elements[0] );
     }

     /* Create a new region. */
     ret = dfb_layer_region_create( layer, &region );
     if (ret)
          return ret;

     fusion_vector_insert( &shared->regions, region, 0 );

     /* allocate the surface before enabling it */
     if (shared->layer_info.desc.caps & DLCAPS_SURFACE) {
          ret = allocate_surface( layer, region, &shared->config );
          if (ret) {
               fusion_vector_remove( &shared->regions, 0 );
               dfb_layer_region_unref( region );
               return ret;
          }
     }

     /* set default/last configuration, this shouldn't fail */
     ret = funcs->SetConfiguration( layer, layer->driver_data,
                                    layer->layer_data, &shared->config );
     if (ret) {
          ERRORMSG("DirectFB/Core/layers: "
                   "Setting default/last configuration failed!\n");

          if (region->surface)
               deallocate_surface( layer, region );

          fusion_vector_remove( &shared->regions, 0 );
          dfb_layer_region_unref( region );

          return ret;
     }

     shared->last_config = shared->config;

     /* enable the display layer */
     ret = funcs->Enable( layer, layer->driver_data, layer->layer_data );
     if (ret) {
          if (region->surface)
               deallocate_surface( layer, region );

          fusion_vector_remove( &shared->regions, 0 );
          dfb_layer_region_unref( region );

          return ret;
     }

     shared->enabled = true;

     /* create a window stack on layers with support */
     if (shared->layer_info.desc.caps & (DLCAPS_SURFACE | DLCAPS_WINDOWS)) {
          shared->stack = dfb_windowstack_new( layer,
                                               shared->config.width,
                                               shared->config.height );

          if (shared->config.buffermode == DLBM_WINDOWS) {
               shared->stack->hw_mode = true;
          }
          else {
               /* clear the layer's surface */
               dfb_windowstack_repaint_all( shared->stack );
          }
     }

     INITMSG( "DirectFB/Layer: Enabled '%s'.\n", shared->layer_info.desc.name );

     return DFB_OK;
}

DFBResult
dfb_layer_disable( DisplayLayer *layer )
{
     DFBResult           ret;
     DisplayLayerShared *shared;
     CoreLayerRegion    *region;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );

     shared = layer->shared;

     /* FIXME: add reference counting */
     if (!shared->enabled)
          return DFB_OK;

     DFB_ASSUME( shared->regions.num_elements > 0 );

     if (!shared->regions.num_elements)
          return DFB_OK;

     region = shared->regions.elements[0];

     /* call driver's disable function */
     ret = layer->funcs->Disable( layer,
                                  layer->driver_data, layer->layer_data );
     if (ret && ret != DFB_UNSUPPORTED)
          return ret;

     /* destroy the window stack if there is one */
     if (shared->stack) {
          CoreWindowStack *stack = shared->stack;

          shared->stack = NULL;

          /* detach listener from background surface and unlink it */
          if (stack->bg.image) {
               dfb_surface_detach_global( stack->bg.image, &shared->bgimage_reaction );
               dfb_surface_unlink( stack->bg.image );
          }

          dfb_windowstack_destroy( stack );
     }

     /* deallocate the surface */
     ret = deallocate_surface( layer, region );
     if (ret) {
          ERRORMSG("DirectFB/Core/layers: Surface deallocation failed!\n");
          return ret;
     }

     fusion_vector_remove( &shared->regions, 0 );

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
     DFBDisplayLayerConfigFlags  unchanged;
     DisplayLayerShared         *shared;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( config != NULL );

     unchanged = ~(config->flags);
     shared    = layer->shared;

     /*
      * Fill all unchanged values with their current setting.
      */
     if (unchanged & DLCONF_BUFFERMODE)
          config->buffermode = shared->config.buffermode;
     else if (config->buffermode == DLBM_WINDOWS &&
              !(shared->layer_info.desc.caps & DLCAPS_WINDOWS))
     {
          if (failed)
               *failed = DLCONF_BUFFERMODE;

          return DFB_UNSUPPORTED;
     }

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
     DisplayLayerShared *shared;
     CoreLayerRegion    *region;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( layer->shared->regions.num_elements > 0 );
     DFB_ASSERT( config != NULL );

     shared = layer->shared;
     region = shared->regions.elements[0];

     /* build new configuration and test it */
     ret = dfb_layer_test_configuration( layer, config, NULL );
     if (ret)
          return ret;

     /* reallocate the surface before setting the new configuration */
     if (shared->layer_info.desc.caps & DLCAPS_SURFACE) {
          ret = reallocate_surface( layer, region, config );
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

     /* reset palette */
     if (region->surface) {
          CoreSurface *surface = region->surface;

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

     if (!shared->exclusive) {
          CoreWindowStack *stack = shared->stack;

          if (stack) {
               /* Update hardware flag. */
               stack->hw_mode = (shared->config.buffermode == DLBM_WINDOWS);

               /* Tell the windowing core about the new size. */
               dfb_windowstack_resize( stack, config->width, config->height );
          }

          /* Backup configuration of shared access. */
          shared->last_config = shared->config;
     }

     return DFB_OK;
}

DFBResult
dfb_layer_get_configuration( DisplayLayer          *layer,
                             DFBDisplayLayerConfig *config )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( config != NULL );

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
     DisplayLayerShared *shared;
     CoreWindowStack    *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     shared = layer->shared;
     stack  = shared->stack;

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
     DisplayLayerShared *shared;
     CoreWindowStack    *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( image != NULL );

     shared = layer->shared;
     stack  = shared->stack;

     /* if the surface is changed */
     if (stack->bg.image != image) {
          CoreSurface *old_image = stack->bg.image;

          /* link surface object */
          dfb_surface_link( &stack->bg.image, image );

          /* detach listener from old surface and unlink it */
          if (old_image) {
               dfb_surface_detach_global( old_image, &shared->bgimage_reaction );
               dfb_surface_unlink( old_image );
          }

          /* attach listener to new surface */
          dfb_surface_attach_global( image, DFB_LAYER_BACKGROUND_IMAGE_LISTENER,
                                     (void*)layer->shared->id,
                                     &shared->bgimage_reaction );
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
     DisplayLayerShared *shared;
     CoreWindowStack    *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( color != NULL );

     shared = layer->shared;
     stack  = shared->stack;

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
     DisplayLayerShared *shared;
     CoreLayerRegion    *region;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );

     shared = layer->shared;

     DFB_ASSUME( shared->regions.num_elements > 0 );

     if (!shared->regions.num_elements)
          return NULL;

     region = shared->regions.elements[0];

     DFB_ASSUME( region->surface != NULL );

     return region->surface;
}

CardState *
dfb_layer_state( DisplayLayer *layer )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     return &layer->state;
}

void
dfb_layer_description( const DisplayLayer         *layer,
                       DFBDisplayLayerDescription *desc )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( desc != NULL );

     *desc = layer->shared->layer_info.desc;
}

DFBDisplayLayerID
dfb_layer_id( const DisplayLayer *layer )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );

     return layer->shared->id;
}

DFBDisplayLayerID
dfb_layer_id_translated( const DisplayLayer *layer )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );

     if (dfb_config->primary_layer > 0 &&
         dfb_config->primary_layer < layersfield->num)
     {
          if (layer->shared->id == DLID_PRIMARY)
               return dfb_config->primary_layer;

          if (layer->shared->id == dfb_config->primary_layer)
               return DLID_PRIMARY;
     }

     return layer->shared->id;
}

DFBResult
dfb_layer_flip_buffers( DisplayLayer *layer, DFBSurfaceFlipFlags flags )
{
     DisplayLayerShared *shared;
     CoreLayerRegion    *region;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( layer->shared->regions.num_elements > 0 );

     shared = layer->shared;
     region = shared->regions.elements[0];

     if (!region->surface)
          return DFB_UNSUPPORTED;

     switch (shared->config.buffermode) {
          case DLBM_FRONTONLY:
               return DFB_UNSUPPORTED;

          case DLBM_TRIPLE:
          case DLBM_BACKVIDEO:
               return layer->funcs->FlipBuffers( layer,
                                                 layer->driver_data,
                                                 layer->layer_data, flags );

          case DLBM_BACKSYSTEM:
               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC)
                    dfb_layer_wait_vsync( layer );

               dfb_back_to_front_copy( region->surface, NULL );
               dfb_layer_update_region( layer, NULL, flags );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAIT)
                    dfb_layer_wait_vsync( layer );
               break;

          default:
               BUG("unknown buffer mode");
               return DFB_BUG;
     }

     return DFB_OK;
}

DFBResult
dfb_layer_update_region( DisplayLayer        *layer,
                         DFBRegion           *region,
                         DFBSurfaceFlipFlags  flags )
{
     DFB_ASSERT( layer );
     DFB_ASSERT( layer->funcs );
     DFB_ASSERT( layer->shared );
     DFB_ASSERT( layer->shared->enabled );

     if (layer->funcs->UpdateRegion)
          return layer->funcs->UpdateRegion( layer,
                                             layer->driver_data,
                                             layer->layer_data,
                                             region, flags );

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
                         CoreWindow            **ret_window )
{
     DFBResult        ret;
     CoreWindow      *window;
     CoreWindowStack *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( layer->shared->stack );
     DFB_ASSERT( ret_window != NULL );

     stack = layer->shared->stack;

     if (fusion_skirmish_prevail( &stack->lock ))
         return DFB_FUSION;

     if (!stack->cursor.set)
          dfb_layer_cursor_enable( layer, true );

     ret = dfb_window_create( stack, layer, x, y, width, height,
                              caps, surface_caps, pixelformat,
                              &layer->shared->last_config, &window );
     if (ret) {
          fusion_skirmish_dismiss( &stack->lock );
          return ret;
     }

     *ret_window = window;

     fusion_skirmish_dismiss( &stack->lock );

     return DFB_OK;
}

CoreWindow *dfb_layer_find_window( DisplayLayer *layer, DFBWindowID id )
{
     int               i;
     CoreWindowStack  *stack;
     int               num;
     CoreWindow       *window = NULL;
     CoreWindow      **windows;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( layer->shared->stack );

     stack = layer->shared->stack;

     if (fusion_skirmish_prevail( &stack->lock ))
         return NULL;

     num     = stack->num_windows;
     windows = stack->windows;

     for (i=0; i<num; i++) {
          if (windows[i]->id == id) {
               window = windows[i];
               break;
          }
     }

     if (window && dfb_window_ref( window ))
          window = NULL;

     fusion_skirmish_dismiss( &stack->lock );

     return window;
}

DFBResult
dfb_layer_set_src_colorkey( DisplayLayer *layer,
                            __u8 r, __u8 g, __u8 b )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     if (!layer->funcs->SetSrcColorKey)
          return DFB_UNSUPPORTED;

     return layer->funcs->SetSrcColorKey( layer, layer->driver_data,
                                          layer->layer_data, r, g, b );
}

DFBResult
dfb_layer_set_dst_colorkey( DisplayLayer *layer,
                            __u8 r, __u8 g, __u8 b )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     if (!layer->funcs->SetDstColorKey)
          return DFB_UNSUPPORTED;

     return layer->funcs->SetDstColorKey( layer, layer->driver_data,
                                          layer->layer_data, r, g, b );
}

DFBResult
dfb_layer_get_level( DisplayLayer *layer, int *level )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( level != NULL );

     if (!layer->funcs->GetLevel)
          return DFB_UNSUPPORTED;

     return layer->funcs->GetLevel( layer, layer->driver_data,
                                    layer->layer_data, level );
}

DFBResult
dfb_layer_set_level( DisplayLayer *layer, int level )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

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
     DisplayLayerShared *shared;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     shared = layer->shared;

     if (shared->screen.x == x     && shared->screen.y == y &&
         shared->screen.w == width && shared->screen.h == height)
          return DFB_OK;

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
     DisplayLayerShared *shared;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     shared = layer->shared;

     if (shared->opacity == opacity)
          return DFB_OK;

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
dfb_layer_get_current_output_field( DisplayLayer *layer, int *field )
{
     DFBResult ret;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( field != NULL );

     if (!layer->funcs->GetCurrentOutputField)
          return DFB_UNSUPPORTED;

     ret = layer->funcs->GetCurrentOutputField( layer, layer->driver_data,
                                                layer->layer_data, field );
     if (ret)
          return ret;

     return DFB_OK;
}

DFBResult
dfb_layer_set_field_parity( DisplayLayer *layer, int field )
{
     DFBResult ret;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     if (!layer->funcs->SetFieldParity)
          return DFB_UNSUPPORTED;

     ret = layer->funcs->SetFieldParity( layer, layer->driver_data,
                                         layer->layer_data, field );
     if (ret)
          return ret;

     return DFB_OK;
}

DFBResult
dfb_layer_wait_vsync( DisplayLayer *layer )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     if (!layer->funcs->WaitVSync)
          return DFB_UNSUPPORTED;

     return layer->funcs->WaitVSync( layer, layer->driver_data,
                                     layer->layer_data );
}

DFBResult
dfb_layer_set_screen_power_mode( DisplayLayer       *layer,
                                 DFBScreenPowerMode  mode )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );

     if (!layer->funcs->SetScreenPowerMode)
          return DFB_UNSUPPORTED;

     return layer->funcs->SetScreenPowerMode( layer, layer->driver_data,
                                              layer->layer_data, mode );
}


DFBResult
dfb_layer_set_coloradjustment (DisplayLayer       *layer,
                               DFBColorAdjustment *adj)
{
     DFBResult                ret;
     DisplayLayerShared      *shared;
     DFBColorAdjustmentFlags  unchanged;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( adj != NULL );

     shared    = layer->shared;
     unchanged = ~adj->flags & shared->adjustment.flags;

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
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( adj != NULL );

     *adj = layer->shared->adjustment;

     return DFB_OK;
}

DFBResult
dfb_layer_get_cursor_position (DisplayLayer       *layer,
                               int                *x,
                               int                *y)
{
     DisplayLayerShared *shared;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->stack != NULL );

     shared = layer->shared;

     if (x)
          *x = shared->stack->cursor.x;

     if (y)
          *y = shared->stack->cursor.y;

     return DFB_OK;
}

DFBSurfacePixelFormat
dfb_primary_layer_pixelformat()
{
     DisplayLayer *layer = dfb_layer_at_translated(DLID_PRIMARY);

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );

     return layer->shared->config.pixelformat;
}

void
dfb_primary_layer_rectangle( float x, float y,
                             float w, float h,
                             DFBRectangle *rect )
{
     DisplayLayer       *layer  = dfb_layers[0];
     DisplayLayerShared *shared;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );

     shared = layer->shared;

     rect->x = (int)(x * (float)shared->config.width + 0.5f);
     rect->y = (int)(y * (float)shared->config.height + 0.5f);
     rect->w = (int)(w * (float)shared->config.width + 0.5f);
     rect->h = (int)(h * (float)shared->config.height + 0.5f);
}

/*
 * hardware window support
 */
DFBResult
dfb_layer_add_window( DisplayLayer *layer,
                      CoreWindow   *window )
{
     DFBResult          ret;
     DisplayLayerFuncs *funcs;
     void              *window_data;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( window != NULL );
     DFB_ASSERT( window->window_data == NULL );

     funcs = layer->funcs;

     if (!funcs->AddWindow || !funcs->WindowDataSize)
          return DFB_UNSUPPORTED;

     window_data = SHCALLOC( 1, funcs->WindowDataSize() );
     if (!window_data)
          return DFB_NOSYSTEMMEMORY;

     ret = funcs->AddWindow( layer, layer->driver_data,
                             layer->layer_data, window_data, window );
     if (ret) {
          ERRORMSG( "DirectFB/core/layers: "
                    "AddWindow (%d, %d - %dx%d) failed!\n",
                    window->x, window->y, window->width, window->height );

          SHFREE( window_data );

          return ret;
     }

     window->window_data = window_data;

     return DFB_OK;
}

DFBResult
dfb_layer_update_window( DisplayLayer          *layer,
                         CoreWindow            *window,
                         CoreWindowUpdateFlags  flags )
{
     DFBResult          ret;
     DisplayLayerFuncs *funcs;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( window != NULL );
     DFB_ASSERT( window->window_data != NULL );

     funcs = layer->funcs;

     if (!funcs->UpdateWindow)
          return DFB_UNSUPPORTED;

     ret = funcs->UpdateWindow( layer, layer->driver_data, layer->layer_data,
                                window->window_data, window, flags );
     if (ret) {
          ERRORMSG( "DirectFB/core/layers: "
                    "UpdateWindow (%d, %d - %dx%d -> 0x%08x) failed!\n",
                    window->x, window->y, window->width, window->height, flags);
          return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_layer_remove_window( DisplayLayer *layer,
                         CoreWindow   *window )
{
     DFBResult          ret;
     DisplayLayerFuncs *funcs;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( window != NULL );
     DFB_ASSERT( window->window_data != NULL );

     funcs = layer->funcs;

     if (!funcs->RemoveWindow)
          return DFB_UNSUPPORTED;

     ret = funcs->RemoveWindow( layer, layer->driver_data, layer->layer_data,
                                window->window_data, window );
     if (ret) {
          ERRORMSG( "DirectFB/core/layers: "
                    "RemoveWindow (%d, %d - %dx%d) failed!\n",
                    window->x, window->y, window->width, window->height );
     }

     SHFREE( window->window_data );
     window->window_data = NULL;

     return DFB_OK;
}


/*
 * cursor control
 */

DFBResult
dfb_layer_cursor_enable( DisplayLayer *layer, bool enable )
{
     CoreWindowStack *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->stack != NULL );
     DFB_ASSERT( layer->shared->enabled );

     stack = layer->shared->stack;

     if (fusion_skirmish_prevail( &stack->lock ))
         return DFB_FUSION;

     stack->cursor.set = true;

     if (dfb_config->no_cursor) {
          fusion_skirmish_dismiss( &stack->lock );
          return DFB_OK;
     }

     if (enable) {
          if (!stack->cursor.window) {
               DFBResult ret;

               ret = load_default_cursor( layer );
               if (ret) {
                    fusion_skirmish_dismiss( &stack->lock );
                    return ret;
               }
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

     fusion_skirmish_dismiss( &stack->lock );

     return DFB_OK;
}

DFBResult
dfb_layer_cursor_set_opacity( DisplayLayer *layer, __u8 opacity )
{
     CoreWindowStack *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->stack != NULL );
     DFB_ASSERT( layer->shared->enabled );

     stack = layer->shared->stack;

     if (fusion_skirmish_prevail( &stack->lock ))
         return DFB_FUSION;

     if (stack->cursor.enabled) {
          DFB_ASSERT( stack->cursor.window );

          dfb_window_set_opacity( stack->cursor.window, opacity );
     }

     stack->cursor.opacity = opacity;

     fusion_skirmish_dismiss( &stack->lock );

     return DFB_OK;
}

DFBResult
dfb_layer_cursor_set_shape( DisplayLayer *layer,
                            CoreSurface  *shape,
                            int           hot_x,
                            int           hot_y )
{
     DFBResult        ret;
     int              dx, dy;
     CoreWindowStack *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->stack != NULL );
     DFB_ASSERT( layer->shared->enabled );
     DFB_ASSERT( shape != NULL );

     if (dfb_config->no_cursor)
          return DFB_OK;

     stack = layer->shared->stack;

     if (fusion_skirmish_prevail( &stack->lock ))
         return DFB_FUSION;

     if (!stack->cursor.window) {
          ret = create_cursor_window( layer, shape->width, shape->height );
          if (ret) {
              fusion_skirmish_dismiss( &stack->lock );
              return ret;
          }
     }
     else if (stack->cursor.window->width != shape->width  ||
              stack->cursor.window->height != shape->height) {
          ret = dfb_window_resize( stack->cursor.window,
                                   shape->width, shape->height );
          if (ret) {
               fusion_skirmish_dismiss( &stack->lock );
               return ret;
          }
     }

     dfb_gfx_copy( shape, stack->cursor.window->surface, NULL );

     dx = stack->cursor.x - hot_x - stack->cursor.window->x;
     dy = stack->cursor.y - hot_y - stack->cursor.window->y;

     if (dx || dy)
          dfb_window_move( stack->cursor.window, dx, dy );
     else
          dfb_window_repaint( stack->cursor.window, NULL, 0, false, false );

     fusion_skirmish_dismiss( &stack->lock );

     return DFB_OK;
}

DFBResult
dfb_layer_cursor_warp( DisplayLayer *layer, int x, int y )
{
     int              dx, dy;
     CoreWindowStack *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->stack != NULL );
     DFB_ASSERT( layer->shared->enabled );

     stack = layer->shared->stack;

     if (fusion_skirmish_prevail( &stack->lock ))
         return DFB_FUSION;

     dx = x - stack->cursor.x;
     dy = y - stack->cursor.y;

     dfb_windowstack_handle_motion( stack, dx, dy );

     fusion_skirmish_dismiss( &stack->lock );

     return DFB_OK;
}

DFBResult
dfb_layer_cursor_set_acceleration( DisplayLayer *layer,
                                   int           numerator,
                                   int           denominator,
                                   int           threshold )
{
     CoreWindowStack *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->stack != NULL );
     DFB_ASSERT( layer->shared->enabled );

     stack = layer->shared->stack;

     stack->cursor.numerator   = numerator;
     stack->cursor.denominator = denominator;
     stack->cursor.threshold   = threshold;

     return DFB_OK;
}

CoreWindowStack *
dfb_layer_window_stack( DisplayLayer *layer )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
/* FIXME:     DFB_ASSERT( layer->shared->stack != NULL );
     DFB_ASSERT( layer->shared->enabled ); */

     return layer->shared->stack;
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
     int                 pitch;
     void               *data;
     FILE               *f      = NULL;
     DisplayLayerShared *shared;
     CoreWindowStack    *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->stack != NULL );
     DFB_ASSERT( layer->shared->enabled );

     shared = layer->shared;
     stack  = shared->stack;

     if (!stack->cursor.window) {
          ret = create_cursor_window( layer, 40, 40 );
          if (ret)
               return ret;
     }

     /* lock the surface of the window */
     ret = dfb_surface_soft_lock( stack->cursor.window->surface,
                                  DSLF_WRITE, &data, &pitch, 0 );
     if (ret) {
          ERRORMSG( "DirectFB/core/layers: "
                    "cannot lock the surface for cursor window data!\n" );
          return ret;
     }

     /* initialize as empty cursor */
     memset( data, 0, 40 * pitch);

     /* open the file containing the cursors image data */
     f = fopen( CURSORFILE, "rb" );
     if (!f) {
          ret = errno2dfb( errno );

          /* ignore a missing cursor file */
          if (ret == DFB_FILENOTFOUND)
               ret = DFB_OK;
          else
               PERRORMSG( "`" CURSORFILE "` could not be opened!\n" );

          goto finish;
     }

     /* read from file directly into the cursor window surface */
     for (i=0; i<40; i++) {
          if (fread( data, MIN (40*4, pitch), 1, f ) != 1) {
               ret = errno2dfb( errno );

               ERRORMSG( "DirectFB/core/layers: "
                         "unexpected end or read error of cursor data!\n" );

               goto finish;
          }
#ifdef WORDS_BIGENDIAN
          {
               int i = MIN (40, pitch/4);
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

 finish:
     if (f) fclose( f );
     dfb_surface_unlock( stack->cursor.window->surface, 0 );

     dfb_window_repaint( stack->cursor.window, NULL, 0, false, false );

     return ret;
}

static DFBResult
create_cursor_window( DisplayLayer *layer,
                      int           width,
                      int           height )
{
     DFBResult           ret;
     CoreWindow         *cursor;
     DisplayLayerShared *shared;
     CoreWindowStack    *stack;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->stack != NULL );
     DFB_ASSERT( layer->shared->enabled );

     shared = layer->shared;
     stack  = shared->stack;

     /* reinitialization check */
     if (stack->cursor.window) {
          BUG( "already created a cursor for this layer" );
          return DFB_BUG;
     }

     stack->cursor.opacity = 0xFF;
     stack->cursor.x = shared->config.width / 2;
     stack->cursor.y = shared->config.height / 2;

     /* create a super-top-most_event-and-focus-less window */
     ret = dfb_window_create( stack, layer,
                              stack->cursor.x,
                              stack->cursor.y, width, height,
                              DWHC_TOPMOST | DWCAPS_ALPHACHANNEL,
                              DSCAPS_NONE, DSPF_UNKNOWN,
                              &layer->shared->last_config, &cursor );
     if (ret) {
          ERRORMSG( "DirectFB/Core/layers: "
                    "Failed creating a window for software cursor!\n" );
          return ret;
     }

     cursor->events   = 0;
     cursor->options |= DWOP_GHOST;

     dfb_window_link( &stack->cursor.window, cursor );

     dfb_window_unref( cursor );

     dfb_window_init( cursor );
     dfb_window_set_opacity( cursor, stack->cursor.opacity );

     return DFB_OK;
}


/*
 * layer surface (re/de)allocation
 */

static DFBResult
allocate_surface( DisplayLayer          *layer,
                  CoreLayerRegion       *region,
                  DFBDisplayLayerConfig *config )
{
     DFBResult               ret;
     DFBSurfaceCapabilities  caps   = DSCAPS_VIDEOONLY;
     DisplayLayerShared     *shared;
     DisplayLayerFuncs      *funcs;
     CoreSurface            *surface = NULL;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( region != NULL );
     DFB_ASSERT( region->surface == NULL );

     shared = layer->shared;
     funcs  = layer->funcs;

     if (config->buffermode == DLBM_WINDOWS)
          return DFB_OK;

     if (funcs->AllocateSurface) {
          ret = funcs->AllocateSurface( layer, layer->driver_data,
                                        layer->layer_data, config, &surface );
          if (ret) {
               ERRORMSG( "DirectFB/core/layers: AllocateSurface() failed!\n" );
               return ret;
          }
     }
     else {
          /* choose buffermode */
          if (config->flags & DLCONF_BUFFERMODE) {
               switch (config->buffermode) {
                    case DLBM_FRONTONLY:
                         break;

                    case DLBM_TRIPLE:
                         caps |= DSCAPS_TRIPLE;
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

          ret = dfb_surface_create( config->width, config->height,
                                    config->pixelformat, CSP_VIDEOONLY,
                                    caps, NULL, &surface );
          if (ret) {
               ERRORMSG( "DirectFB/core/layers: Surface creation failed!\n" );
               return ret;
          }
     }

     dfb_layer_region_set_surface( region, surface );

     dfb_surface_unref( surface );

     return DFB_OK;
}

static DFBResult
reallocate_surface( DisplayLayer          *layer,
                    CoreLayerRegion       *region,
                    DFBDisplayLayerConfig *config )
{
     DFBResult           ret;
     DisplayLayerShared *shared;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( region != NULL );

     shared = layer->shared;

     if (layer->funcs->ReallocateSurface)
          return layer->funcs->ReallocateSurface( layer, layer->driver_data,
                                                  layer->layer_data, config,
                                                  region->surface );

     /* FIXME: write surface management functions
               for easier configuration changes */

     if (shared->config.buffermode != config->buffermode) {
          if (shared->config.buffermode == DLBM_WINDOWS)
               return allocate_surface( layer, region, config );

          DFB_ASSERT( region->surface != NULL );

          switch (config->buffermode) {
               case DLBM_TRIPLE:
                    region->surface->caps |= DSCAPS_TRIPLE;
                    region->surface->caps &= ~DSCAPS_FLIPPING;
                    ret = dfb_surface_reconfig( region->surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;
               case DLBM_BACKVIDEO:
                    region->surface->caps |= DSCAPS_FLIPPING;
                    region->surface->caps &= ~DSCAPS_TRIPLE;
                    ret = dfb_surface_reconfig( region->surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;
               case DLBM_BACKSYSTEM:
                    region->surface->caps |= DSCAPS_FLIPPING;
                    region->surface->caps &= ~DSCAPS_TRIPLE;
                    ret = dfb_surface_reconfig( region->surface,
                                                CSP_VIDEOONLY, CSP_SYSTEMONLY );
                    break;
               case DLBM_FRONTONLY:
                    region->surface->caps &= ~(DSCAPS_FLIPPING | DSCAPS_TRIPLE);
                    ret = dfb_surface_reconfig( region->surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;
               case DLBM_WINDOWS:
                    return deallocate_surface( layer, region );

               default:
                    BUG("unknown buffermode");
                    return DFB_BUG;
          }

          if (ret)
               return ret;
     }

     if (region->surface) {
          ret = dfb_surface_reformat( region->surface, config->width,
                                      config->height, config->pixelformat );
          if (ret)
               return ret;

          if (config->options & DLOP_DEINTERLACING)
               region->surface->caps |= DSCAPS_INTERLACED;
          else
               region->surface->caps &= ~DSCAPS_INTERLACED;
     }

     return DFB_OK;
}

static DFBResult
deallocate_surface( DisplayLayer *layer, CoreLayerRegion *region )
{
     DisplayLayerShared *shared;
     DisplayLayerFuncs  *funcs;
     CoreSurface        *surface;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( region != NULL );
     DFB_ASSERT( region->surface != NULL );

     shared  = layer->shared;
     funcs   = layer->funcs;
     surface = region->surface;

     if (surface) {
          dfb_layer_region_set_surface( region, NULL );

          if (funcs->DeallocateSurface)
               return funcs->DeallocateSurface( layer, layer->driver_data,
                                                layer->layer_data, surface );
     }

     return DFB_OK;
}


/*
 * listen to the background image
 */
ReactionResult
_dfb_layer_background_image_listener( const void *msg_data,
                                      void       *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     DFBDisplayLayerID              layer_id     = (DFBDisplayLayerID) ctx;
     DisplayLayer                  *layer        = dfb_layer_at( layer_id );
     CoreWindowStack               *stack        = layer->shared->stack;

     if (!stack)
          return RS_REMOVE;

     if (notification->flags & CSNF_DESTROY) {
          if (stack->bg.image == notification->surface) {
               ERRORMSG("DirectFB/core/layers: Surface for background vanished.\n");

               stack->bg.mode  = DLBM_COLOR;
               stack->bg.image = NULL;

               dfb_windowstack_repaint_all( stack );
          }

          return RS_REMOVE;
     }

     if (notification->flags & (CSNF_FLIP | CSNF_SIZEFORMAT))
          dfb_windowstack_repaint_all( stack );

     return RS_OK;
}

