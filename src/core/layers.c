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
#include <core/layer_context.h>
#include <core/layer_control.h>
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



typedef struct {
     int              num;
     CoreLayerShared *layers[MAX_LAYERS];
} CoreLayersField;

static CoreLayersField *layersfield = NULL;

static int        dfb_num_layers = 0;
static CoreLayer *dfb_layers[MAX_LAYERS] = { NULL };


DFB_CORE_PART( layers, 0, sizeof(CoreLayersField) )


/** public **/

static DFBResult
dfb_layers_initialize( CoreDFB *core, void *data_local, void *data_shared )
{
     int       i;
     DFBResult ret;

     DFB_ASSERT( layersfield == NULL );
     DFB_ASSERT( data_shared != NULL );

     layersfield = data_shared;

     /* Initialize all registered layers. */
     for (i=0; i<dfb_num_layers; i++) {
          //CoreLayerContext  *context;
          CoreLayerShared   *shared;
          CoreLayer         *layer = dfb_layers[i];
          DisplayLayerFuncs *funcs = layer->funcs;

          /* Allocate shared data. */
          shared = SHCALLOC( 1, sizeof(CoreLayerShared) );

          /* Assign ID (zero based index). */
          shared->layer_id = i;

          /* Initialize the lock. */
          if (fusion_skirmish_init( &shared->lock )) {
               SHFREE( shared );
               return DFB_FUSION;
          }

          /* Allocate driver's layer data. */
          if (funcs->LayerDataSize) {
               int size = funcs->LayerDataSize();

               if (size > 0) {
                    shared->layer_data = SHCALLOC( 1, size );
                    if (!shared->layer_data) {
                         fusion_skirmish_destroy( &shared->lock );
                         SHFREE( shared );
                         return DFB_NOSYSTEMMEMORY;
                    }
               }
          }

          /* Initialize the layer, get the layer description,
             the default configuration and default color adjustment. */
          ret = funcs->InitLayer( layer,
                                  layer->driver_data,
                                  shared->layer_data,
                                  &shared->description,
                                  &shared->default_config,
                                  &shared->default_adjustment );
          if (ret) {
               ERRORMSG("DirectFB/Core/layers: "
                        "Failed to initialize layer %d!\n", shared->layer_id);

               fusion_skirmish_destroy( &shared->lock );

               if (shared->layer_data)
                    SHFREE( shared->layer_data );

               SHFREE( shared );

               return ret;
          }

          /* Initialize the vector for the contexts. */
          fusion_vector_init( &shared->contexts.stack, 4 );

          /* No active context by default. */
          shared->contexts.active = -1;

          /* Store layer data. */
          layer->layer_data = shared->layer_data;

          /* Store pointer to shared data and core. */
          layer->shared = shared;
          layer->core   = core;

          /* Add the layerto the shared list. */
          layersfield->layers[ layersfield->num++ ] = shared;
     }

     return DFB_OK;
}

static DFBResult
dfb_layers_join( CoreDFB *core, void *data_local, void *data_shared )
{
     int i;

     DFB_ASSERT( layersfield == NULL );
     DFB_ASSERT( data_shared != NULL );

     layersfield = data_shared;

     if (dfb_num_layers != layersfield->num) {
          ERRORMSG("DirectFB/core/layers: Number of layers does not match!\n");
          return DFB_BUG;
     }

     for (i=0; i<dfb_num_layers; i++) {
          CoreLayer       *layer  = dfb_layers[i];
          CoreLayerShared *shared = layersfield->layers[i];

          /* make a copy for faster access */
          layer->layer_data = shared->layer_data;

          /* store pointer to shared data and core */
          layer->shared = shared;
          layer->core   = core;
     }

     return DFB_OK;
}

static DFBResult
dfb_layers_shutdown( CoreDFB *core, bool emergency )
{
     int i;

     DFB_ASSERT( layersfield != NULL );

     /* Begin with the most recently added layer. */
     for (i=dfb_num_layers-1; i>=0; i--) {
          CoreLayer       *layer  = dfb_layers[i];
          CoreLayerShared *shared = layer->shared;

          /* Deinitialize the lock. */
          fusion_skirmish_destroy( &shared->lock );

          /* Deinitialize the state for window stack repaints. */
          dfb_state_destroy( &layer->state );

          /* Deinitialize the vector for the contexts. */
          fusion_vector_destroy( &shared->contexts.stack );

          /* Free the driver's layer data. */
          if (shared->layer_data)
               SHFREE( shared->layer_data );

          /* Free the shared layer data. */
          SHFREE( shared );

          /* Free the local layer data. */
          DFBFREE( layer );
     }

     layersfield = NULL;

     dfb_num_layers = 0;

     return DFB_OK;
}

static DFBResult
dfb_layers_leave( CoreDFB *core, bool emergency )
{
     int i;

     DFB_ASSERT( layersfield != NULL );

     /* Deinitialize all local stuff. */
     for (i=0; i<dfb_num_layers; i++) {
          CoreLayer *layer = dfb_layers[i];

          /* Deinitialize the state for window stack repaints. */
          dfb_state_destroy( &layer->state );

          /* Free local layer data. */
          DFBFREE( layer );
     }

     layersfield = NULL;

     dfb_num_layers = 0;

     return DFB_OK;
}

static DFBResult
dfb_layers_suspend( CoreDFB *core )
{
     int i;

     DFB_ASSERT( layersfield != NULL );

     DEBUGMSG( "DirectFB/core/layers: suspending...\n" );

     for (i=dfb_num_layers-1; i>=0; i--)
          dfb_layer_suspend( dfb_layers[i] );

     DEBUGMSG( "DirectFB/core/layers: suspended.\n" );

     return DFB_OK;
}

static DFBResult
dfb_layers_resume( CoreDFB *core )
{
     int i;

     DFB_ASSERT( layersfield != NULL );

     DEBUGMSG( "DirectFB/core/layers: resuming...\n" );

     for (i=0; i<dfb_num_layers; i++)
          dfb_layer_resume( dfb_layers[i] );

     DEBUGMSG( "DirectFB/core/layers: resumed.\n" );

     return DFB_OK;
}

void
dfb_layers_register( GraphicsDevice    *device,
                     void              *driver_data,
                     DisplayLayerFuncs *funcs )
{
     CoreLayer *layer;

     DFB_ASSERT( funcs != NULL );

     if (dfb_num_layers == MAX_LAYERS) {
          ERRORMSG( "DirectFB/Core/Layers: "
                    "Maximum number of layers reached!\n" );
          return;
     }

     /* allocate local data */
     layer = DFBCALLOC( 1, sizeof(CoreLayer) );

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
     int        i;
     int        entries;
     CoreLayer *primary = dfb_layers[0];

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
     CoreLayer *primary = dfb_layers[0];

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

     for (i=0; i<dfb_num_layers; i++) {
          if (callback( dfb_layers[i], ctx ) == DFENUM_CANCEL)
               break;
     }
}

inline CoreLayer *
dfb_layer_at( DFBDisplayLayerID id )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( id >= 0);
     DFB_ASSERT( id < dfb_num_layers);

     return dfb_layers[id];
}

inline CoreLayer *
dfb_layer_at_translated( DFBDisplayLayerID id )
{
     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( id >= 0);
     DFB_ASSERT( id < dfb_num_layers);
     DFB_ASSERT( dfb_config != NULL );

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

inline void
dfb_layer_get_description( const CoreLayer            *layer,
                           DFBDisplayLayerDescription *desc )
{
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( desc != NULL );

     *desc = layer->shared->description;
}

inline DFBDisplayLayerID
dfb_layer_id( const CoreLayer *layer )
{
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );

     return layer->shared->layer_id;
}

DFBDisplayLayerID
dfb_layer_id_translated( const CoreLayer *layer )
{
     CoreLayerShared *shared;

     DFB_ASSERT( layersfield != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( dfb_config != NULL );

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

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );

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

