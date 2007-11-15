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

#include <unistd.h>
#include <string.h>

#include <directfb.h>

#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/screen.h>
#include <core/surface.h>
#include <core/system.h>
#include <core/windows.h>
#include <core/windowstack.h>
#include <core/wm.h>

#include <core/layers_internal.h>
#include <core/windows_internal.h>

#include <fusion/shmalloc.h>


D_DEBUG_DOMAIN( Core_Layers, "Core/Layers", "DirectFB Display Layer Core" );


/**********************************************************************************************************************/

static void      init_region_config  ( CoreLayerContext            *context,
                                       CoreLayerRegionConfig       *config );

static void      build_updated_config( CoreLayer                   *layer,
                                       CoreLayerContext            *context,
                                       const DFBDisplayLayerConfig *update,
                                       CoreLayerRegionConfig       *ret_config,
                                       CoreLayerRegionConfigFlags  *ret_flags );

static DFBResult allocate_surface    ( CoreLayer                   *layer,
                                       CoreLayerRegion             *region,
                                       CoreLayerRegionConfig       *config );

static DFBResult reallocate_surface  ( CoreLayer                   *layer,
                                       CoreLayerRegion             *region,
                                       CoreLayerRegionConfig       *config );

static DFBResult deallocate_surface  ( CoreLayer                   *layer,
                                       CoreLayerRegion             *region );

static void      screen_rectangle    ( CoreLayerContext            *context,
                                       const DFBLocation           *location,
                                       DFBRectangle                *rect );

/**********************************************************************************************************************/

static void
context_destructor( FusionObject *object, bool zombie, void *ctx )
{
     CoreLayerContext *context = (CoreLayerContext*) object;
     CoreLayer        *layer   = dfb_layer_at( context->layer_id );
     CoreLayerShared  *shared  = layer->shared;

     (void) shared;

     D_DEBUG_AT( Core_Layers, "destroying context %p (%s, %sactive%s)\n",
                 context, shared->description.name, context->active ? "" : "in",
                 zombie ? " - ZOMBIE" : "");

     /* Remove the context from the layer's context stack. */
     dfb_layer_remove_context( layer, context );

     /* Destroy the window stack. */
     if (context->stack) {
          dfb_windowstack_destroy( context->stack );
          context->stack = NULL;
     }

     /* Destroy the region vector. */
     fusion_vector_destroy( &context->regions );

     /* Deinitialize the lock. */
     fusion_skirmish_destroy( &context->lock );

     /* Free clip regions. */
     if (context->primary.config.clips)
          SHFREE( context->shmpool, context->primary.config.clips );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

/**********************************************************************************************************************/

FusionObjectPool *
dfb_layer_context_pool_create( const FusionWorld *world )
{
     return fusion_object_pool_create( "Layer Context Pool",
                                       sizeof(CoreLayerContext),
                                       sizeof(CoreLayerContextNotification),
                                       context_destructor, NULL, world );
}

/**********************************************************************************************************************/

DFBResult
dfb_layer_context_init( CoreLayerContext *context,
                        CoreLayer        *layer )
{
     CoreLayerShared  *shared;

     D_ASSERT( context != NULL );
     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     shared = layer->shared;

     D_DEBUG_AT( Core_Layers, "%s -> %p\n", __FUNCTION__, context );

     context->shmpool = shared->shmpool;

     /* Initialize the lock. */
     if (fusion_skirmish_init( &context->lock, "Layer Context", dfb_core_world(layer->core) )) {
          fusion_object_destroy( &context->object );
          return DFB_FUSION;
     }

     /* Initialize the region vector. */
     fusion_vector_init( &context->regions, 4, context->shmpool );

     /* Store layer ID, default configuration and default color adjustment. */
     context->layer_id   = shared->layer_id;
     context->config     = shared->default_config;
     context->adjustment = shared->default_adjustment;
     context->rotation   = dfb_config->layers[dfb_layer_id_translated(layer)].rotate;

     /* Initialize screen location. */
     context->screen.location.x = 0.0f;
     context->screen.location.y = 0.0f;
     context->screen.location.w = 1.0f;
     context->screen.location.h = 1.0f;

     if (D_FLAGS_IS_SET( shared->description.caps, DLCAPS_SCREEN_LOCATION ))
          context->screen.mode = CLLM_LOCATION;
     else if (D_FLAGS_IS_SET( shared->description.caps, DLCAPS_SCREEN_POSITION ))
          context->screen.mode = CLLM_CENTER;

     /* Initialize the primary region's configuration. */
     init_region_config( context, &context->primary.config );

     /* Change global reaction lock. */
     fusion_object_set_lock( &context->object, &context->lock );

     /* Activate the object. */
     fusion_object_activate( &context->object );


     /* Create the window stack. */
     context->stack = dfb_windowstack_create( context );
     if (!context->stack) {
          dfb_layer_context_unref( context );
          return D_OOSHM();
     }

     /* Tell the window stack about its size. */
     dfb_windowstack_resize( context->stack,
                             context->config.width,
                             context->config.height );

     return DFB_OK;
}

DFBResult
dfb_layer_context_activate( CoreLayerContext *context )
{
     DFBResult        ret;
     int              index;
     CoreLayer       *layer;
     CoreLayerRegion *region;

     D_DEBUG_AT( Core_Layers, "%s (%p)\n", __FUNCTION__, context );

     D_ASSERT( context != NULL );

     layer = dfb_layer_at( context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     D_ASSUME( !context->active );

     if (context->active) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Iterate through all regions. */
     fusion_vector_foreach (region, index, context->regions) {
          /* Activate each region. */
          if (dfb_layer_region_activate( region ))
               D_WARN( "could not activate region!" );

          if (region->surface) {
               D_ASSERT( region->surface_lock.buffer == NULL );

               ret = reallocate_surface( layer, region, &region->config );
               if (ret)
                    D_DERROR( ret, "Core/Layers: Reallocation of layer surface failed!\n" );
          }
     }

     context->active = true;

     /* set new adjustment */
     if (layer->funcs->SetColorAdjustment)
          layer->funcs->SetColorAdjustment( layer, layer->driver_data,
                                            layer->layer_data, &context->adjustment );

     /* Resume window stack. */
     if (context->stack)
          dfb_wm_set_active( context->stack, true );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_deactivate( CoreLayerContext *context )
{
     int              index;
     CoreLayerRegion *region;

     D_ASSERT( context != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     D_DEBUG_AT( Core_Layers, "%s (%p)\n", __FUNCTION__, context );

     D_ASSUME( context->active );

     if (!context->active) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Iterate through all regions. */
     fusion_vector_foreach (region, index, context->regions) {
          /* Deactivate each region. */
          dfb_layer_region_deactivate( region );
     }

     context->active = false;

     /* Suspend window stack. */
     if (context->stack)
          dfb_wm_set_active( context->stack, false );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_add_region( CoreLayerContext *context,
                              CoreLayerRegion  *region )
{
     D_ASSERT( context != NULL );
     D_ASSERT( region != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     D_ASSUME( ! fusion_vector_contains( &context->regions, region ) );

     if (fusion_vector_contains( &context->regions, region )) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Add region to vector. */
     if (fusion_vector_add( &context->regions, region )) {
          dfb_layer_context_unlock( context );
          return DFB_FUSION;
     }

     /* Inherit state from context. */
     if (context->active)
          region->state |= CLRSF_ACTIVE;

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_remove_region( CoreLayerContext *context,
                                 CoreLayerRegion  *region )
{
     int index;

     D_ASSERT( context != NULL );
     D_ASSERT( region != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     D_ASSUME( fusion_vector_contains( &context->regions, region ) );

     /* Lookup region. */
     index = fusion_vector_index_of( &context->regions, region );
     if (index < 0) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Remove region from vector. */
     fusion_vector_remove( &context->regions, index );

     /* Check if the primary region is removed. */
     if (region == context->primary.region)
          context->primary.region = NULL;

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_get_primary_region( CoreLayerContext  *context,
                                      bool               create,
                                      CoreLayerRegion  **ret_region )
{
     DFBResult ret = DFB_OK;

     D_ASSERT( context != NULL );
     D_ASSERT( ret_region != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

restart:
     while (context->primary.region) {
         /* Increase the primary region's reference counter. */
         ret = dfb_layer_region_ref( context->primary.region );
         if (ret == DFB_OK)
              break;

         dfb_layer_context_unlock( context );

         if (ret == DFB_LOCKED) {
              //sched_yield();
              usleep( 10000 );

              if (dfb_layer_context_lock( context ))
                   return DFB_FUSION;
         }
         else
              return DFB_FUSION;
     }

     if (!context->primary.region) {
          if (create) {
              CoreLayerRegion *region;

              /* Unlock the context. */
              dfb_layer_context_unlock( context );

              /* Create the primary region. */
              ret = dfb_layer_region_create( context, &region );
              if (ret) {
                   D_ERROR( "DirectFB/core/layers: Could not create primary region!\n" );
                   return ret;
              }

              /* Lock the context again. */
              if (dfb_layer_context_lock( context )) {
                   dfb_layer_region_unref( region );
                   return DFB_FUSION;
              }

              /* Check for race. */
              if (context->primary.region) {
                   dfb_layer_region_unref( region );
                   goto restart;
              }

              /* Set the region configuration. */
              ret = dfb_layer_region_set_configuration( region,
                                                        &context->primary.config,
                                                        CLRCF_ALL );
              if (ret) {
                   D_DERROR( ret, "DirectFB/core/layers: "
                             "Could not set primary region config!\n" );
                   dfb_layer_region_unref( region );
                   dfb_layer_context_unlock( context );
                   return ret;
              }

              /* Remember the primary region. */
              context->primary.region = region;

              /* Allocate surface, enable region etc. */
              ret = dfb_layer_context_set_configuration( context, &context->config );
              if (ret) {
                   D_DERROR( ret, "DirectFB/core/layers: "
                             "Could not set layer context config!\n" );
                   context->primary.region = NULL;
                   dfb_layer_region_unref( region );
                   dfb_layer_context_unlock( context );
                   return ret;
              }
          }
          else {
               dfb_layer_context_unlock( context );
               return DFB_TEMPUNAVAIL;
          }
     }

     /* Return region. */
     *ret_region = context->primary.region;

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

/*
 * configuration management
 */
DFBResult
dfb_layer_context_test_configuration( CoreLayerContext            *context,
                                      const DFBDisplayLayerConfig *config,
                                      DFBDisplayLayerConfigFlags  *ret_failed )
{
     DFBResult                   ret = DFB_OK;
     CoreLayer                  *layer;
     CoreLayerRegionConfig       region_config;
     CoreLayerRegionConfigFlags  failed;
     const DisplayLayerFuncs    *funcs;

     D_ASSERT( context != NULL );
     D_ASSERT( config != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     layer = dfb_layer_at( context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( layer->funcs->TestRegion != NULL );

     funcs = layer->funcs;

     /* Build a new region configuration with the changes. */
     build_updated_config( layer, context, config, &region_config, NULL );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );


     /* Test the region configuration. */
     if (region_config.buffermode == DLBM_WINDOWS) {
          if (! D_FLAGS_IS_SET( layer->shared->description.caps, DLCAPS_WINDOWS )) {
               failed = CLRCF_BUFFERMODE;
               ret = DFB_UNSUPPORTED;
          }
     }
     else {
          /* Let the driver examine the modified configuration. */
          ret = funcs->TestRegion( layer, layer->driver_data, layer->layer_data,
                                   &region_config, &failed );
     }

     /* Return flags for failing entries. */
     if (ret_failed) {
          DFBDisplayLayerConfigFlags flags = DLCONF_NONE;

          /* Translate flags. */
          if (ret != DFB_OK) {
               if (failed & CLRCF_WIDTH)
                    flags |= DLCONF_WIDTH;

               if (failed & CLRCF_HEIGHT)
                    flags |= DLCONF_HEIGHT;

               if (failed & CLRCF_FORMAT)
                    flags |= DLCONF_PIXELFORMAT;

               if (failed & CLRCF_BUFFERMODE)
                    flags |= DLCONF_BUFFERMODE;

               if (failed & CLRCF_OPTIONS)
                    flags |= DLCONF_OPTIONS;

               if (failed & CLRCF_SOURCE_ID)
                    flags |= DLCONF_SOURCE;

               if (failed & CLRCF_SURFACE_CAPS)
                    flags |= DLCONF_SURFACE_CAPS;
          }

          *ret_failed = flags;
     }

     return ret;
}

DFBResult
dfb_layer_context_set_configuration( CoreLayerContext            *context,
                                     const DFBDisplayLayerConfig *config )
{
     int                         i;
     DFBResult                   ret;
     CoreLayer                  *layer;
     CoreLayerShared            *shared;
     CoreLayerRegionConfig       region_config;
     CoreLayerRegionConfigFlags  flags;
     const DisplayLayerFuncs    *funcs;

     D_DEBUG_AT( Core_Layers, "%s (%p, %p)\n", __FUNCTION__, context, config );

     D_DEBUG_AT( Core_Layers, "%s (%p, %p)\n", __FUNCTION__, context, config );

     D_ASSERT( context != NULL );
     D_ASSERT( config != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     layer  = dfb_layer_at( context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( layer->funcs->TestRegion != NULL );

     shared = layer->shared;
     funcs  = layer->funcs;

     /* Build a new region configuration with the changes. */
     build_updated_config( layer, context, config, &region_config, &flags );

     /* Test the region configuration first. */
     if (region_config.buffermode == DLBM_WINDOWS) {
          if (! D_FLAGS_IS_SET( shared->description.caps, DLCAPS_WINDOWS )) {
               dfb_layer_context_unlock( context );
               return DFB_UNSUPPORTED;
          }
     }
     else {
          ret = funcs->TestRegion( layer, layer->driver_data, layer->layer_data,
                                   &region_config, NULL );
          if (ret) {
               dfb_layer_context_unlock( context );
               return ret;
          }
     }

     /* Set the region configuration. */
     if (context->primary.region) {
          CoreLayerRegion *region = context->primary.region;

          /* Add local reference. */
          if (dfb_layer_region_ref( region )) {
               dfb_layer_context_unlock( context );
               return DFB_FUSION;
          }

          /* Lock the region. */
          if (dfb_layer_region_lock( region )) {
               dfb_layer_region_unref( region );
               dfb_layer_context_unlock( context );
               return DFB_FUSION;
          }

          /* Normal buffer mode? */
          if (region_config.buffermode != DLBM_WINDOWS) {
               bool                      surface    = shared->description.caps & DLCAPS_SURFACE;
               CoreLayerRegionStateFlags configured = region->state & CLRSF_CONFIGURED;

               if (shared->description.caps & DLCAPS_SOURCES) {
                    for (i=0; i<shared->description.sources; i++) {
                         if (shared->sources[i].description.source_id == region_config.source_id)
                              break;
                    }

                    D_ASSERT( i < shared->description.sources );

                    surface = shared->sources[i].description.caps & DDLSCAPS_SURFACE;
               }

               D_FLAGS_CLEAR( region->state, CLRSF_CONFIGURED );

               /* (Re)allocate the region's surface. */
               if (surface) {
                    flags |= CLRCF_SURFACE | CLRCF_PALETTE;

                    if (region->surface) {
                         if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
                              if (!D_FLAGS_IS_SET( region->state, CLRSF_FROZEN ))
                                   D_ASSUME( region->surface_lock.buffer != NULL );

                              if (region->surface_lock.buffer)
                                   dfb_surface_unlock_buffer( region->surface, &region->surface_lock );
                         }

                         ret = reallocate_surface( layer, region, &region_config );
                         if (ret)
                              D_DERROR( ret, "Core/Layers: Reallocation of layer surface failed!\n" );
                    }
                    else {
                         ret = allocate_surface( layer, region, &region_config );
                         if (ret)
                              D_DERROR( ret, "Core/Layers: Allocation of layer surface failed!\n" );
                    }

                    if (ret) {
                         dfb_layer_region_unlock( region );
                         dfb_layer_region_unref( region );
                         dfb_layer_context_unlock( context );
                         return ret;
                    }
               }
               else if (region->surface)
                    deallocate_surface( layer, region );

               region->state |= configured;

               /* Set the new region configuration. */
               dfb_layer_region_set_configuration( region, &region_config, flags );

               /* Enable the primary region. */
               if (! D_FLAGS_IS_SET( region->state, CLRSF_ENABLED ))
                    dfb_layer_region_enable( region );
          }
          else {
               /* Disable and deallocate the primary region. */
               if (D_FLAGS_IS_SET( region->state, CLRSF_ENABLED )) {
                    dfb_layer_region_disable( region );

                    if (region->surface)
                         deallocate_surface( layer, region );
               }
          }

          /* Unlock the region and give up the local reference. */
          dfb_layer_region_unlock( region );
          dfb_layer_region_unref( region );
     }

     /* Remember new region config. */
     context->primary.config = region_config;

     /*
      * Write back modified entries.
      */
     if (config->flags & DLCONF_WIDTH)
          context->config.width = config->width;

     if (config->flags & DLCONF_HEIGHT)
          context->config.height = config->height;

     if (config->flags & DLCONF_PIXELFORMAT)
          context->config.pixelformat = config->pixelformat;

     if (config->flags & DLCONF_BUFFERMODE)
          context->config.buffermode = config->buffermode;

     if (config->flags & DLCONF_OPTIONS)
          context->config.options = config->options;

     if (config->flags & DLCONF_SOURCE)
          context->config.source = config->source;

     if (config->flags & DLCONF_SURFACE_CAPS)
          context->config.surface_caps = config->surface_caps;

     /* Update the window stack. */
     if (context->stack) {
          CoreWindowStack *stack = context->stack;

          /* Update hardware flag. */
          stack->hw_mode = (region_config.buffermode == DLBM_WINDOWS);

          /* Tell the windowing core about the new size. */
          if (config->flags & (DLCONF_WIDTH | DLCONF_HEIGHT |
                               DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_SURFACE_CAPS))
          {
               dfb_windowstack_resize( stack,
                                       region_config.width,
                                       region_config.height );

               /* FIXME: call only if really needed */
               dfb_windowstack_repaint_all( stack );
          }
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_get_configuration( CoreLayerContext      *context,
                                     DFBDisplayLayerConfig *config )
{
     D_ASSERT( context != NULL );
     D_ASSERT( config != NULL );

     *config = context->config;

     return DFB_OK;
}

static DFBResult
update_primary_region_config( CoreLayerContext           *context,
                              CoreLayerRegionConfig      *config,
                              CoreLayerRegionConfigFlags  flags )
{
     DFBResult ret = DFB_OK;

     D_ASSERT( context != NULL );
     D_ASSERT( config != NULL );

     if (context->primary.region) {
          /* Set the new configuration. */
          ret = dfb_layer_region_set_configuration( context->primary.region, config, flags );
     }
     else {
          CoreLayer *layer = dfb_layer_at( context->layer_id );

          D_ASSERT( layer->funcs != NULL );
          D_ASSERT( layer->funcs->TestRegion != NULL );

          /* Just test the new configuration. */
          ret = layer->funcs->TestRegion( layer, layer->driver_data,
                                          layer->layer_data, config, NULL );
     }

     if (ret)
          return ret;

     /* Remember the configuration. */
     context->primary.config = *config;

     return DFB_OK;
}

DFBResult
dfb_layer_context_set_src_colorkey( CoreLayerContext *context,
                                    u8 r, u8 g, u8 b, int index )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     D_ASSERT( context != NULL );

     D_DEBUG_AT( Core_Layers, "%s (%02x %02x %02x - %d)\n", __FUNCTION__, r, g, b, index );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Take the current configuration. */
     config = context->primary.config;

     /* Change the color key. */
     config.src_key.r = r;
     config.src_key.g = g;
     config.src_key.b = b;

     if (index >= 0)
          config.src_key.index = index & 0xff;

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_SRCKEY );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_dst_colorkey( CoreLayerContext *context,
                                    u8 r, u8 g, u8 b, int index )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     D_ASSERT( context != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Take the current configuration. */
     config = context->primary.config;

     /* Change the color key. */
     config.dst_key.r = r;
     config.dst_key.g = g;
     config.dst_key.b = b;

     if (index >= 0)
          config.dst_key.index = index & 0xff;

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_DSTKEY );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_sourcerectangle( CoreLayerContext   *context,
                                       const DFBRectangle *source )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     D_ASSERT( context != NULL );
     D_ASSERT( source != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Take the current configuration. */
     config = context->primary.config;

     /* Do nothing if the source rectangle didn't change. */
     if (DFB_RECTANGLE_EQUAL( config.source, *source )) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Check if the new source rectangle is valid. */
     if (source->x < 0 || source->y < 0 ||
         source->x + source->w > config.width ||
         source->y + source->h > config.height) {
          dfb_layer_context_unlock( context );
          return DFB_INVAREA;
     }

     /* Change the source rectangle. */
     config.source = *source;

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_SOURCE );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_screenlocation( CoreLayerContext  *context,
                                      const DFBLocation *location )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     D_ASSERT( context != NULL );
     D_ASSERT( location != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Do nothing if the location didn't change. */
/*     if (context->screen.mode == CLLM_LOCATION &&
         DFB_LOCATION_EQUAL( context->screen.location, *location ))
     {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }*/

     /* Take the current configuration. */
     config = context->primary.config;

     /* Calculate new absolute screen coordinates. */
     screen_rectangle( context, location, &config.dest );

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_DEST );
     if (ret == DFB_OK) {
          context->screen.location  = *location;
          context->screen.rectangle = config.dest;
          context->screen.mode      = CLLM_LOCATION;
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_screenrectangle( CoreLayerContext   *context,
                                       const DFBRectangle *rectangle )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     D_ASSERT( context != NULL );

     DFB_RECTANGLE_ASSERT( rectangle );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Do nothing if the location didn't change. */
/*     if (context->screen.mode == CLLM_RECTANGLE &&
         DFB_RECTANGLE_EQUAL( context->screen.rectangle, *rectangle ))
     {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }*/

     /* Take the current configuration. */
     config = context->primary.config;

     /* Use supplied absolute screen coordinates. */
     config.dest = *rectangle;

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_DEST );
     if (ret == DFB_OK) {
          context->screen.rectangle = config.dest;
          context->screen.mode      = CLLM_RECTANGLE;
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_screenposition( CoreLayerContext  *context,
                                      int                x,
                                      int                y )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     D_ASSERT( context != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Do nothing if the location didn't change. */
     if (context->primary.config.dest.x == x && context->primary.config.dest.y == y) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Take the current configuration. */
     config = context->primary.config;

     /* Set new absolute screen coordinates. */
     config.dest.x = x;
     config.dest.y = y;

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_DEST );
     if (ret == DFB_OK) {
          context->screen.rectangle = config.dest;
          context->screen.mode      = CLLM_POSITION;
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_opacity( CoreLayerContext *context,
                               u8                opacity )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     D_ASSERT( context != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Do nothing if the opacity didn't change. */
     if (context->primary.config.opacity == opacity) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Take the current configuration. */
     config = context->primary.config;

     /* Change the opacity. */
     config.opacity = opacity;

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_OPACITY );
     if (ret == DFB_OK)
          context->primary.config.opacity = opacity;

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_rotation( CoreLayerContext *context,
                                int               rotation )
{
     D_ASSERT( context != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Do nothing if the rotation didn't change. */
     if (context->rotation != rotation) {
          context->rotation = rotation;

          dfb_windowstack_repaint_all( context->stack );
     }

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return DFB_OK;
}

DFBResult
dfb_layer_context_set_coloradjustment( CoreLayerContext         *context,
                                       const DFBColorAdjustment *adjustment )
{
     DFBResult           ret;
     DFBColorAdjustment  adj;
     CoreLayer          *layer;

     D_ASSERT( context != NULL );
     D_ASSERT( adjustment != NULL );

     layer = dfb_layer_at( context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );

     adj = context->adjustment;

     if (!layer->funcs->SetColorAdjustment)
          return DFB_UNSUPPORTED;

     /* if flags are set that are not in the default adjustment */
     if (adjustment->flags & ~context->adjustment.flags)
          return DFB_UNSUPPORTED;

     /* take over changed values */
     if (adjustment->flags & DCAF_BRIGHTNESS)  adj.brightness = adjustment->brightness;
     if (adjustment->flags & DCAF_CONTRAST)    adj.contrast   = adjustment->contrast;
     if (adjustment->flags & DCAF_HUE)         adj.hue        = adjustment->hue;
     if (adjustment->flags & DCAF_SATURATION)  adj.saturation = adjustment->saturation;

     /* set new adjustment */
     ret = layer->funcs->SetColorAdjustment( layer, layer->driver_data,
                                             layer->layer_data, &adj );
     if (ret)
          return ret;

     /* keep new adjustment */
     context->adjustment = adj;

     return DFB_OK;
}

DFBResult
dfb_layer_context_get_coloradjustment( CoreLayerContext   *context,
                                       DFBColorAdjustment *ret_adjustment )
{
     D_ASSERT( context != NULL );
     D_ASSERT( ret_adjustment != NULL );

     *ret_adjustment = context->adjustment;

     return DFB_OK;
}

DFBResult
dfb_layer_context_set_field_parity( CoreLayerContext *context,
                                    int               field )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     D_ASSERT( context != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Do nothing if the parity didn't change. */
     if (context->primary.config.parity == field) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Take the current configuration. */
     config = context->primary.config;

     /* Change the parity. */
     config.parity = field;

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_PARITY );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_clip_regions( CoreLayerContext *context,
                                    const DFBRegion  *regions,
                                    int               num_regions,
                                    DFBBoolean        positive )
{
     DFBResult              ret;
     CoreLayerRegionConfig  config;
     DFBRegion             *clips;
     DFBRegion             *old_clips;

     D_ASSERT( context != NULL );

     clips = SHMALLOC( context->shmpool, sizeof(DFBRegion) * num_regions );
     if (!clips)
          return D_OOSHM();

     direct_memcpy( clips, regions, sizeof(DFBRegion) * num_regions );

     /* Lock the context. */
     if (dfb_layer_context_lock( context )) {
          SHFREE( context->shmpool, clips );
          return DFB_FUSION;
     }

     /* Take the current configuration. */
     config = context->primary.config;

     /* Remember for freeing later on. */
     old_clips = config.clips;

     /* Change the clip regions. */
     config.clips     = clips;
     config.num_clips = num_regions;
     config.positive  = positive;

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_CLIPS );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     if (ret)
          SHFREE( context->shmpool, clips );
     else if (old_clips)
          SHFREE( context->shmpool, old_clips );

     return ret;
}

DFBResult
dfb_layer_context_create_window( CoreDFB                     *core,
                                 CoreLayerContext            *context,
                                 const DFBWindowDescription  *desc,
                                 CoreWindow                 **ret_window )
{
     DFBResult        ret;
     CoreWindow      *window;
     CoreWindowStack *stack;
     CoreLayer       *layer;

     D_DEBUG_AT( Core_Layers, "%s()\n", __FUNCTION__ );

     D_ASSERT( context != NULL );
     D_ASSERT( context->stack != NULL );
     D_ASSERT( desc != NULL );
     D_ASSERT( ret_window != NULL );

     layer = dfb_layer_at( context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );

     if (dfb_layer_context_lock( context ))
         return DFB_FUSION;

     stack = context->stack;

     if (!stack->cursor.set) {
          ret = dfb_windowstack_cursor_enable( core, stack, true );
          if (ret) {
               dfb_layer_context_unlock( context );
               return ret;
          }
     }

     ret = dfb_window_create( stack, desc, &window );
     if (ret) {
          dfb_layer_context_unlock( context );
          return ret;
     }

     *ret_window = window;

     dfb_layer_context_unlock( context );

     return DFB_OK;
}

CoreWindow *
dfb_layer_context_find_window( CoreLayerContext *context, DFBWindowID id )
{
     CoreWindowStack *stack;
     CoreWindow      *window;

     D_ASSERT( context != NULL );
     D_ASSERT( context->stack != NULL );

     stack = context->stack;

     if (dfb_layer_context_lock( context ))
         return NULL;

     if (dfb_wm_window_lookup( stack, id, &window ) || dfb_window_ref( window ))
          window = NULL;

     dfb_layer_context_unlock( context );

     return window;
}

CoreWindowStack *
dfb_layer_context_windowstack( const CoreLayerContext *context )
{
     D_ASSERT( context != NULL );

     return context->stack;
}

bool
dfb_layer_context_active( const CoreLayerContext *context )
{
     D_ASSERT( context != NULL );

     return context->active;
}

DirectResult
dfb_layer_context_lock( CoreLayerContext *context )
{
     D_ASSERT( context != NULL );

     return fusion_skirmish_prevail( &context->lock );
}

DirectResult
dfb_layer_context_unlock( CoreLayerContext *context )
{
     D_ASSERT( context != NULL );

     return fusion_skirmish_dismiss( &context->lock );
}

/**************************************************************************************************/

/*
 * region config construction
 */
static void
init_region_config( CoreLayerContext      *context,
                    CoreLayerRegionConfig *config )
{
     D_ASSERT( context != NULL );
     D_ASSERT( config  != NULL );

     memset( config, 0, sizeof(CoreLayerRegionConfig) );

     /* Initialize values from layer config. */
     config->width        = context->config.width;
     config->height       = context->config.height;
     config->format       = context->config.pixelformat;
     config->buffermode   = context->config.buffermode;
     config->options      = context->config.options;
     config->source_id    = context->config.source;
     config->surface_caps = context->config.surface_caps;

     /* Initialize source rectangle. */
     config->source.x   = 0;
     config->source.y   = 0;
     config->source.w   = config->width;
     config->source.h   = config->height;

     /* Initialize screen rectangle. */
     screen_rectangle( context, &context->screen.location, &config->dest );

     /* Set default opacity. */
     config->opacity = 0xff;

     /* Set default alpha ramp. */
     config->alpha_ramp[0] = 0x00;
     config->alpha_ramp[1] = 0x55;
     config->alpha_ramp[2] = 0xaa;
     config->alpha_ramp[3] = 0xff;
}

static void
build_updated_config( CoreLayer                   *layer,
                      CoreLayerContext            *context,
                      const DFBDisplayLayerConfig *update,
                      CoreLayerRegionConfig       *ret_config,
                      CoreLayerRegionConfigFlags  *ret_flags )
{
     CoreLayerRegionConfigFlags flags = CLRCF_NONE;

     D_ASSERT( layer != NULL );
     D_ASSERT( context != NULL );
     D_ASSERT( update != NULL );
     D_ASSERT( ret_config != NULL );

     /* Get the current region configuration. */
     *ret_config = context->primary.config;

     /* Change width. */
     if (update->flags & DLCONF_WIDTH) {
          flags |= CLRCF_WIDTH;
          ret_config->width  = update->width;
     }

     /* Change height. */
     if (update->flags & DLCONF_HEIGHT) {
          flags |= CLRCF_HEIGHT;
          ret_config->height = update->height;
     }

     /* Update source and destination rectangle. */
     if (update->flags & (DLCONF_WIDTH | DLCONF_HEIGHT)) {
          int width, height;

          flags |= CLRCF_SOURCE | CLRCF_DEST;

          ret_config->source.x = 0;
          ret_config->source.y = 0;
          ret_config->source.w = ret_config->width;
          ret_config->source.h = ret_config->height;

          switch (context->screen.mode) {
               case CLLM_CENTER:
                    dfb_screen_get_layer_dimension( layer->screen, layer, &width, &height );

                    ret_config->dest.x = (width  - ret_config->width)  / 2;
                    ret_config->dest.y = (height - ret_config->height) / 2;
                    /* fall through */

               case CLLM_POSITION:
                    ret_config->dest.w = ret_config->width;
                    ret_config->dest.h = ret_config->height;
                    break;

               case CLLM_LOCATION:
               case CLLM_RECTANGLE:
                    break;

               default:
                    D_BREAK( "invalid layout mode" );
          }
     }

     /* Change pixel format. */
     if (update->flags & DLCONF_PIXELFORMAT) {
          flags |= CLRCF_FORMAT;
          ret_config->format = update->pixelformat;
     }

     /* Change buffer mode. */
     if (update->flags & DLCONF_BUFFERMODE) {
          flags |= CLRCF_BUFFERMODE;
          ret_config->buffermode = update->buffermode;
     }

     /* Change options. */
     if (update->flags & DLCONF_OPTIONS) {
          flags |= CLRCF_OPTIONS;
          ret_config->options = update->options;
     }

     /* Change source id. */
     if (update->flags & DLCONF_SOURCE) {
          flags |= CLRCF_SOURCE_ID;
          ret_config->source_id = update->source;
     }

     /* Change surface caps. */
     if (update->flags & DLCONF_SURFACE_CAPS) {
          flags |= CLRCF_SURFACE_CAPS;
          ret_config->surface_caps = update->surface_caps;
     }

     /* Return translated flags. */
     if (ret_flags)
          *ret_flags = flags;
}

/*
 * region surface (re/de)allocation
 */
static DFBResult
allocate_surface( CoreLayer             *layer,
                  CoreLayerRegion       *region,
                  CoreLayerRegionConfig *config )
{
     DFBResult                ret;
     const DisplayLayerFuncs *funcs;
     CoreSurface             *surface = NULL;
     DFBSurfaceCapabilities   caps    = DSCAPS_VIDEOONLY;
     CoreSurfaceTypeFlags     type    = CSTF_LAYER;
     CoreSurfaceConfig        scon;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( region != NULL );
     D_ASSERT( region->surface == NULL );
     D_ASSERT( config != NULL );
     D_ASSERT( config->buffermode != DLBM_WINDOWS );

     funcs = layer->funcs;

     /*
      * Create a new surface for the region.
      * Drivers may provide their own surface creation (unusual).
      */
     if (funcs->AllocateSurface) {
          /* Let the driver create the surface. */
          ret = funcs->AllocateSurface( layer, layer->driver_data,
                                        layer->layer_data, region->region_data,
                                        config, &surface );
          if (ret) {
               D_ERROR( "DirectFB/core/layers: AllocateSurface() failed!\n" );
               return ret;
          }
     }
     else {
          CoreLayerShared *shared = layer->shared;

          /* Choose surface capabilities depending on the buffer mode. */
          switch (config->buffermode) {
               case DLBM_FRONTONLY:
                    break;

               case DLBM_BACKVIDEO:
               case DLBM_BACKSYSTEM:
                    caps |= DSCAPS_DOUBLE;
                    break;

               case DLBM_TRIPLE:
                    caps |= DSCAPS_TRIPLE;
                    break;

               default:
                    D_BUG("unknown buffermode");
                    break;
          }

          /* FIXME: remove this? */
          if (config->options & DLOP_DEINTERLACING)
               caps |= DSCAPS_INTERLACED;

          /* Add available surface capabilities. */
          caps |= config->surface_caps & (DSCAPS_INTERLACED |
                                          DSCAPS_SEPARATED  |
                                          DSCAPS_PREMULTIPLIED);

          scon.flags  = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
          scon.size.w = config->width;
          scon.size.h = config->height;
          scon.format = config->format;
          scon.caps   = caps;

          if (shared->contexts.primary == region->context)
               type |= CSTF_SHARED;

          /* Use the default surface creation. */
          ret = dfb_surface_create( layer->core, &scon, type, shared->layer_id, NULL, &surface );
          if (ret) {
               D_DERROR( ret, "Core/layers: Surface creation failed!\n" );
               return ret;
          }

          if (config->buffermode == DLBM_BACKSYSTEM)
               surface->buffers[1]->policy = CSP_SYSTEMONLY;
     }

     /* Tell the region about its new surface (adds a global reference). */
     ret = dfb_layer_region_set_surface( region, surface );

     /* Remove local reference of dfb_surface_create(). */
     dfb_surface_unref( surface );

     return ret;
}

static DFBResult
reallocate_surface( CoreLayer             *layer,
                    CoreLayerRegion       *region,
                    CoreLayerRegionConfig *config )
{
     DFBResult                ret;
     const DisplayLayerFuncs *funcs;
     CoreSurface             *surface;
     CoreSurfaceConfig        sconfig;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( region != NULL );
     D_ASSERT( region->surface != NULL );
     D_ASSERT( config != NULL );
     D_ASSERT( config->buffermode != DLBM_WINDOWS );

     funcs   = layer->funcs;
     surface = region->surface;

     if (funcs->ReallocateSurface)
          return funcs->ReallocateSurface( layer, layer->driver_data,
                                           layer->layer_data,
                                           region->region_data,
                                           config, surface );

     sconfig.flags = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;

     sconfig.caps = surface->config.caps & ~(DSCAPS_FLIPPING  | DSCAPS_INTERLACED |
                                             DSCAPS_SEPARATED | DSCAPS_PREMULTIPLIED);

     switch (config->buffermode) {
          case DLBM_TRIPLE:
               sconfig.caps |= DSCAPS_TRIPLE;
               break;

          case DLBM_BACKVIDEO:
          case DLBM_BACKSYSTEM:
               sconfig.caps |= DSCAPS_DOUBLE;
               break;

          case DLBM_FRONTONLY:
               break;

          default:
               D_BUG("unknown buffermode");
               return DFB_BUG;
     }

     /* Add available surface capabilities. */
     sconfig.caps |= config->surface_caps & (DSCAPS_INTERLACED |
                                             DSCAPS_SEPARATED  |
                                             DSCAPS_PREMULTIPLIED); 

     if (config->options & DLOP_DEINTERLACING)
          sconfig.caps |= DSCAPS_INTERLACED;

     sconfig.size.w = config->width;
     sconfig.size.h = config->height;
     sconfig.format = config->format;

     ret = dfb_surface_lock( surface );
     if (ret)
          return ret;

     ret = dfb_surface_reconfig( surface, &sconfig );
     if (ret) {
          dfb_surface_unlock( surface );
          return ret;
     }

     if (DFB_PIXELFORMAT_IS_INDEXED(surface->config.format) && !surface->palette) {
          ret = dfb_surface_init_palette( layer->core, surface );
          if (ret)
               D_DERROR( ret, "Core/Layers: Could not initialize palette while switching to indexed mode!\n" );
     }

     if (config->buffermode == DLBM_BACKSYSTEM)
          surface->buffers[1]->policy = CSP_SYSTEMONLY;

     dfb_surface_unlock( surface );
     
     return DFB_OK;
}

static DFBResult
deallocate_surface( CoreLayer *layer, CoreLayerRegion *region )
{
     DFBResult                ret;
     const DisplayLayerFuncs *funcs;
     CoreSurface             *surface;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( region != NULL );

     D_ASSUME( region->surface != NULL );

     funcs   = layer->funcs;
     surface = region->surface;

     if (surface) {
          /* Special deallocation by the driver. */
          if (funcs->DeallocateSurface) {
               ret = funcs->DeallocateSurface( layer, layer->driver_data,
                                               layer->layer_data,
                                               region->region_data, surface );
               if (ret)
                    return ret;
          }

          /* Detach the global listener. */
          dfb_surface_detach_global( surface, &region->surface_reaction );

          /* Unlink from structure. */
          dfb_surface_unlink( &region->surface );
     }

     return DFB_OK;
}

static void
screen_rectangle( CoreLayerContext  *context,
                  const DFBLocation *location,
                  DFBRectangle      *rect )
{
     DFBResult  ret;
     int        width;
     int        height;
     CoreLayer *layer;

     D_ASSERT( context != NULL );

     layer = dfb_layer_at( context->layer_id );

     D_ASSERT( layer->screen != NULL );

     ret = dfb_screen_get_layer_dimension( layer->screen, layer, &width, &height );
     if (ret) {
          D_WARN( "could not determine mixer/screen dimension of layer %d", context->layer_id );

          rect->x = location->x * 720;
          rect->y = location->y * 576;
          rect->w = location->w * 720;
          rect->h = location->h * 576;
     }
     else {
          rect->x = location->x * width;
          rect->y = location->y * height;
          rect->w = location->w * width;
          rect->h = location->h * height;
     }
}

