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
#include <errno.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/shmalloc.h>
#include <fusion/arena.h>
#include <fusion/property.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core_parts.h>

#include <core/input.h>
#include <core/gfxcard.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/state.h>
#include <core/palette.h>
#include <core/system.h>
#include <core/windows.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <core/layers_internal.h>
#include <core/screens_internal.h>


D_DEBUG_DOMAIN( Core_Layer, "Core/Layer", "DirectFB Display Layer Core" );

/**********************************************************************************************************************/

typedef struct {
     int               magic;

     int               num;
     CoreLayerShared  *layers[MAX_LAYERS];
} DFBLayerCoreShared;

struct __DFB_DFBLayerCore {
     int                 magic;

     CoreDFB            *core;

     DFBLayerCoreShared *shared;
};


DFB_CORE_PART( layer_core, LayerCore );

/**********************************************************************************************************************/

static int           dfb_num_layers;
static CoreLayer    *dfb_layers[MAX_LAYERS];

/** FIXME: Add proper error paths! **/

static DFBResult
dfb_layer_core_initialize( CoreDFB            *core,
                           DFBLayerCore       *data,
                           DFBLayerCoreShared *shared )
{
     int                  i;
     DFBResult            ret;
     FusionSHMPoolShared *pool;

     D_DEBUG_AT( Core_Layer, "dfb_layer_core_initialize( %p, %p, %p )\n", core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     data->core   = core;
     data->shared = shared;


     pool = dfb_core_shmpool( core );

     /* Initialize all registered layers. */
     for (i=0; i<dfb_num_layers; i++) {
          char                     buf[24];
          CoreLayerShared         *lshared;
          CoreLayer               *layer = dfb_layers[i];
          const DisplayLayerFuncs *funcs = layer->funcs;

          /* Allocate shared data. */
          lshared = SHCALLOC( pool, 1, sizeof(CoreLayerShared) );

          /* Assign ID (zero based index). */
          lshared->layer_id = i;
          lshared->shmpool  = pool;

          snprintf( buf, sizeof(buf), "Display Layer %d", i );

          /* Initialize the lock. */
          ret = fusion_skirmish_init( &lshared->lock, buf, dfb_core_world(core) );
          if (ret)
               return ret;

          /* Allocate driver's layer data. */
          if (funcs->LayerDataSize) {
               int size = funcs->LayerDataSize();

               if (size > 0) {
                    lshared->layer_data = SHCALLOC( pool, 1, size );
                    if (!lshared->layer_data)
                         return D_OOSHM();
               }
          }

          /* Initialize the layer, get the layer description,
             the default configuration and default color adjustment. */
          ret = funcs->InitLayer( layer,
                                  layer->driver_data,
                                  lshared->layer_data,
                                  &lshared->description,
                                  &lshared->default_config,
                                  &lshared->default_adjustment );
          if (ret) {
               D_DERROR( ret, "DirectFB/Core/layers: "
                         "Failed to initialize layer %d!\n", lshared->layer_id );
               return ret;
          }

          if (lshared->description.caps & DLCAPS_SOURCES) {
               int n;

               lshared->sources = SHCALLOC( pool, lshared->description.sources, sizeof(CoreLayerSource) );
               if (!lshared->sources)
                    return D_OOSHM();

               for (n=0; n<lshared->description.sources; n++) {
                    CoreLayerSource *source = &lshared->sources[n];

                    source->index = n;

                    ret = funcs->InitSource( layer, layer->driver_data,
                                             lshared->layer_data, n, &source->description );
                    if (ret) {
                         D_DERROR( ret, "DirectFB/Core/layers: Failed to initialize source %d "
                                   "of layer %d!\n", n, lshared->layer_id );
                         return ret;
                    }
               }
          }

          if (D_FLAGS_IS_SET( lshared->description.caps, DLCAPS_SCREEN_LOCATION ))
               D_FLAGS_SET( lshared->description.caps, DLCAPS_SCREEN_POSITION | DLCAPS_SCREEN_SIZE );

          if (D_FLAGS_ARE_SET( lshared->description.caps,
                               DLCAPS_SCREEN_POSITION | DLCAPS_SCREEN_SIZE ))
               D_FLAGS_SET( lshared->description.caps, DLCAPS_SCREEN_LOCATION );

          /* Initialize the vector for the contexts. */
          fusion_vector_init( &lshared->contexts.stack, 4, pool );

          /* Initialize the vector for realized (added) regions. */
          fusion_vector_init( &lshared->added_regions, 4, pool );

          /* No active context by default. */
          lshared->contexts.active = -1;

          /* Store layer data. */
          layer->layer_data = lshared->layer_data;

          /* Store pointer to shared data and core. */
          layer->shared = lshared;
          layer->core   = core;

          /* Add the layer to the shared list. */
          shared->layers[ shared->num++ ] = lshared;
     }


     D_MAGIC_SET( data, DFBLayerCore );
     D_MAGIC_SET( shared, DFBLayerCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_layer_core_join( CoreDFB            *core,
                     DFBLayerCore       *data,
                     DFBLayerCoreShared *shared )
{
     int i;

     D_DEBUG_AT( Core_Layer, "dfb_layer_core_join( %p, %p, %p )\n", core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBLayerCoreShared );

     data->core   = core;
     data->shared = shared;


     if (dfb_num_layers != shared->num) {
          D_ERROR("DirectFB/core/layers: Number of layers does not match!\n");
          return DFB_BUG;
     }

     for (i=0; i<dfb_num_layers; i++) {
          CoreLayer       *layer   = dfb_layers[i];
          CoreLayerShared *lshared = shared->layers[i];

          /* make a copy for faster access */
          layer->layer_data = lshared->layer_data;

          /* store pointer to shared data and core */
          layer->shared = lshared;
          layer->core   = core;
     }


     D_MAGIC_SET( data, DFBLayerCore );

     return DFB_OK;
}

static DFBResult
dfb_layer_core_shutdown( DFBLayerCore *data,
                         bool          emergency )
{
     int                 i;
     DFBResult           ret;
     DFBLayerCoreShared *shared;

     D_DEBUG_AT( Core_Layer, "dfb_layer_core_shutdown( %p, %semergency )\n", data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBLayerCore );
     D_MAGIC_ASSERT( data->shared, DFBLayerCoreShared );

     shared = data->shared;


     /* Begin with the most recently added layer. */
     for (i=dfb_num_layers-1; i>=0; i--) {
          CoreLayer               *layer  = dfb_layers[i];
          CoreLayerShared         *shared = layer->shared;
          const DisplayLayerFuncs *funcs  = layer->funcs;

          D_ASSUME( emergency || fusion_vector_is_empty( &shared->added_regions ) );

          /* Remove all regions during emergency shutdown. */
          if (emergency && funcs->RemoveRegion) {
               int              n;
               CoreLayerRegion *region;

               fusion_vector_foreach( region, n, shared->added_regions ) {
                   D_DEBUG_AT( Core_Layer, "Removing region (%d, %d - %dx%d) from '%s'.\n",
                               DFB_RECTANGLE_VALS( &region->config.dest ),
                               shared->description.name );

                   ret = funcs->RemoveRegion( layer, layer->driver_data,
                                              layer->layer_data, region->region_data );
                   if (ret)
                        D_DERROR( ret, "Core/Layers: Could not remove region!\n" );
               }
          }

          /* Deinitialize the lock. */
          fusion_skirmish_destroy( &shared->lock );

          /* Deinitialize the state for window stack repaints. */
          dfb_state_destroy( &layer->state );

          /* Deinitialize the vector for the contexts. */
          fusion_vector_destroy( &shared->contexts.stack );

          /* Deinitialize the vector for the realized (added) regions. */
          fusion_vector_destroy( &shared->added_regions );

          /* Free the driver's layer data. */
          if (shared->layer_data)
               SHFREE( shared->shmpool, shared->layer_data );

          /* Free the shared layer data. */
          SHFREE( shared->shmpool, shared );

          /* Free the local layer data. */
          D_FREE( layer );
     }

     dfb_num_layers = 0;


     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

static DFBResult
dfb_layer_core_leave( DFBLayerCore *data,
                      bool          emergency )
{
     int                 i;
     DFBLayerCoreShared *shared;

     D_DEBUG_AT( Core_Layer, "dfb_layer_core_leave( %p, %semergency )\n", data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBLayerCore );
     D_MAGIC_ASSERT( data->shared, DFBLayerCoreShared );

     shared = data->shared;


     /* Deinitialize all local stuff. */
     for (i=0; i<dfb_num_layers; i++) {
          CoreLayer *layer = dfb_layers[i];

          /* Deinitialize the state for window stack repaints. */
          dfb_state_destroy( &layer->state );

          /* Free local layer data. */
          D_FREE( layer );
     }

     dfb_num_layers = 0;


     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
dfb_layer_core_suspend( DFBLayerCore *data )
{
     int                 i;
     DFBLayerCoreShared *shared;

     D_DEBUG_AT( Core_Layer, "dfb_layer_core_suspend( %p )\n", data );

     D_MAGIC_ASSERT( data, DFBLayerCore );
     D_MAGIC_ASSERT( data->shared, DFBLayerCoreShared );

     shared = data->shared;

     for (i=dfb_num_layers-1; i>=0; i--)
          dfb_layer_suspend( dfb_layers[i] );

     return DFB_OK;
}

static DFBResult
dfb_layer_core_resume( DFBLayerCore *data )
{
     int                 i;
     DFBLayerCoreShared *shared;

     D_DEBUG_AT( Core_Layer, "dfb_layer_core_resume( %p )\n", data );

     D_MAGIC_ASSERT( data, DFBLayerCore );
     D_MAGIC_ASSERT( data->shared, DFBLayerCoreShared );

     shared = data->shared;

     for (i=0; i<dfb_num_layers; i++)
          dfb_layer_resume( dfb_layers[i] );

     return DFB_OK;
}

/**********************************************************************************************************************/

CoreLayer *
dfb_layers_register( CoreScreen              *screen,
                     void                    *driver_data,
                     const DisplayLayerFuncs *funcs )
{
     CoreLayer *layer;

     D_ASSERT( screen != NULL );
     D_ASSERT( funcs != NULL );

     if (dfb_num_layers == MAX_LAYERS) {
          D_ERROR( "DirectFB/Core/Layers: "
                    "Maximum number of layers reached!\n" );
          return NULL;
     }

     /* allocate local data */
     layer = D_CALLOC( 1, sizeof(CoreLayer) );

     /* assign local pointers */
     layer->device      = screen->device;
     layer->screen      = screen;
     layer->driver_data = driver_data;
     layer->funcs       = funcs;

     /* Initialize the state for window stack repaints */
     dfb_state_init( &layer->state, NULL );

     /* add it to the local list */
     dfb_layers[dfb_num_layers++] = layer;

     return layer;
}

typedef void (*AnyFunc)();

CoreLayer *
dfb_layers_hook_primary( CoreGraphicsDevice *device,
                         void               *driver_data,
                         DisplayLayerFuncs  *funcs,
                         DisplayLayerFuncs  *primary_funcs,
                         void              **primary_driver_data )
{
     int        i;
     int        entries;
     CoreLayer *primary = dfb_layers[0];

     D_ASSERT( primary != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( funcs != NULL );

     /* copy content of original function table */
     if (primary_funcs)
          direct_memcpy( primary_funcs, primary->funcs, sizeof(DisplayLayerFuncs) );

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

     return primary;
}

CoreLayer *
dfb_layers_replace_primary( CoreGraphicsDevice *device,
                            void               *driver_data,
                            DisplayLayerFuncs  *funcs )
{
     CoreLayer *primary = dfb_layers[0];

     D_ASSERT( primary != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( funcs != NULL );

     /* replace device, function table and driver data pointer */
     primary->device      = device;
     primary->funcs       = funcs;
     primary->driver_data = driver_data;

     return primary;
}

void
dfb_layers_enumerate( DisplayLayerCallback  callback,
                      void                 *ctx )
{
     int i;

     D_ASSERT( callback != NULL );

     for (i=0; i<dfb_num_layers; i++) {
          if (callback( dfb_layers[i], ctx ) == DFENUM_CANCEL)
               break;
     }
}

int
dfb_layer_num()
{
     return dfb_num_layers;
}

CoreLayer *
dfb_layer_at( DFBDisplayLayerID id )
{
     D_ASSERT( id >= 0);
     D_ASSERT( id < dfb_num_layers);

     return dfb_layers[id];
}

CoreLayer *
dfb_layer_at_translated( DFBDisplayLayerID id )
{
     D_ASSERT( id >= 0);
     D_ASSERT( id < dfb_num_layers);
     D_ASSERT( dfb_config != NULL );

     if (dfb_config->primary_layer > 0 &&
         dfb_config->primary_layer < dfb_num_layers)
     {
          if (id == DLID_PRIMARY)
               return dfb_layer_at( dfb_config->primary_layer );

          if (id == dfb_config->primary_layer)
               return dfb_layer_at( DLID_PRIMARY );
     }

     return dfb_layer_at( id );
}

void
dfb_layer_get_description( const CoreLayer            *layer,
                           DFBDisplayLayerDescription *desc )
{
     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( desc != NULL );

     *desc = layer->shared->description;
}

CoreScreen *
dfb_layer_screen( const CoreLayer *layer )
{
     D_ASSERT( layer != NULL );

     return layer->screen;
}

CardState *
dfb_layer_state( CoreLayer *layer )
{
     D_ASSERT( layer != NULL );

     return &layer->state;
}

DFBDisplayLayerID
dfb_layer_id( const CoreLayer *layer )
{
     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     return layer->shared->layer_id;
}

DFBDisplayLayerID
dfb_layer_id_translated( const CoreLayer *layer )
{
     CoreLayerShared *shared;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( dfb_config != NULL );

     shared = layer->shared;

     if (dfb_config->primary_layer > 0 &&
         dfb_config->primary_layer < dfb_num_layers)
     {
          if (shared->layer_id == DLID_PRIMARY)
               return dfb_config->primary_layer;

          if (shared->layer_id == dfb_config->primary_layer)
               return DLID_PRIMARY;
     }

     return shared->layer_id;
}

DFBSurfacePixelFormat
dfb_primary_layer_pixelformat()
{
     CoreLayerShared       *shared;
     CoreLayerContext      *context;
     CoreLayer             *layer  = dfb_layer_at_translated(DLID_PRIMARY);
     DFBSurfacePixelFormat  format = DSPF_UNKNOWN;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     shared = layer->shared;

     /* If no context is active, return the default format. */
     if (dfb_layer_get_active_context( layer, &context ) != DFB_OK)
          return shared->default_config.pixelformat;

     /* Use the format from the current configuration. */
     format = context->config.pixelformat;

     /* Decrease the context's reference counter. */
     dfb_layer_context_unref( context );

     return format;
}

