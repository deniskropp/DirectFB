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

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/screen.h>
#include <core/surfaces.h>
#include <core/system.h>
#include <core/windows.h>
#include <core/windowstack.h>
#include <core/wm.h>

#include <core/layers_internal.h>
#include <core/windows_internal.h>

#include <direct/messages.h>

#include <direct/util.h>


static void      init_region_config  ( CoreLayerContext            *context,
                                       CoreLayerRegionConfig       *config );

static void      build_updated_config( CoreLayerContext            *context,
                                       const DFBDisplayLayerConfig *update,
                                       CoreLayerRegionConfig       *ret_config,
                                       CoreLayerRegionConfigFlags  *ret_flags );

static DFBResult allocate_surface    ( CoreLayer                   *layer,
                                       CoreLayerRegion             *region,
                                       CoreLayerRegionConfig       *config );

static DFBResult reallocate_surface  ( CoreLayer                   *layer,
                                       CoreLayerRegion             *region,
                                       CoreLayerRegionConfig       *config,
                                       DFBDisplayLayerConfig       *previous );

static DFBResult deallocate_surface  ( CoreLayer                   *layer,
                                       CoreLayerRegion             *region );

static void      screen_rectangle    ( CoreLayerContext            *context,
                                       const DFBLocation           *location,
                                       DFBRectangle                *rect );

/******************************************************************************/

static void
context_destructor( FusionObject *object, bool zombie )
{
     CoreLayerContext *context = (CoreLayerContext*) object;
     CoreLayer        *layer   = dfb_layer_at( context->layer_id );
     CoreLayerShared  *shared  = layer->shared;

     (void) shared;

     D_DEBUG("DirectFB/core/layers: destroying context %p (%s, %sactive%s)\n",
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

     /* Destroy the object. */
     fusion_object_destroy( object );
}

/******************************************************************************/

FusionObjectPool *
dfb_layer_context_pool_create()
{
     return fusion_object_pool_create( "Layer Context Pool",
                                       sizeof(CoreLayerContext),
                                       sizeof(CoreLayerContextNotification),
                                       context_destructor );
}

/******************************************************************************/

DFBResult
dfb_layer_context_create( CoreLayer         *layer,
                          CoreLayerContext **ret_context )
{
     CoreLayerShared  *shared;
     CoreLayerContext *context;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( ret_context != NULL );

     shared = layer->shared;

     /* Create the object. */
     context = dfb_core_create_layer_context( layer->core );
     if (!context)
          return DFB_FUSION;

     D_DEBUG( "DirectFB/core/layers: %s -> %p\n", __FUNCTION__, context );

     /* Initialize the lock. */
     if (fusion_skirmish_init( &context->lock, "Layer Context" )) {
          fusion_object_destroy( &context->object );
          return DFB_FUSION;
     }

     /* Initialize the region vector. */
     fusion_vector_init( &context->regions, 4 );

     /* Store layer ID, default configuration and default color adjustment. */
     context->layer_id   = shared->layer_id;
     context->config     = shared->default_config;
     context->adjustment = shared->default_adjustment;

     /* Initialize screen location. */
     context->screen.x = 0.0f;
     context->screen.y = 0.0f;
     context->screen.w = 1.0f;
     context->screen.h = 1.0f;

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
          return DFB_NOSYSTEMMEMORY;
     }

     /* Tell the window stack about its size. */
     dfb_windowstack_resize( context->stack,
                             context->config.width,
                             context->config.height );

     /* Return the new context. */
     *ret_context = context;

     return DFB_OK;
}

DFBResult
dfb_layer_context_activate( CoreLayerContext *context )
{
     int              index;
     CoreLayerRegion *region;

     D_ASSERT( context != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     D_DEBUG( "DirectFB/core/layers: %s (%p)\n", __FUNCTION__, context );

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
     }

     context->active = true;

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

     D_DEBUG( "DirectFB/core/layers: %s (%p)\n", __FUNCTION__, context );

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
     region->state = context->active ? CLRSF_ACTIVE : CLRSF_NONE;

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
     DFBResult ret;

     D_ASSERT( context != NULL );
     D_ASSERT( ret_region != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     if (context->primary.region) {
          /* Increase the primary region's reference counter. */
          if (dfb_layer_region_ref( context->primary.region )) {
               dfb_layer_context_unlock( context );
               return DFB_FUSION;
          }
     }
     else if (create) {
          CoreLayerRegion *region;

          /* Create the primary region. */
          ret = dfb_layer_region_create( context, &region );
          if (ret) {
               D_ERROR( "DirectFB/core/layers: Could not create primary region!\n" );
               dfb_layer_context_unlock( context );
               return ret;
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
          ret = dfb_layer_context_set_configuration( context,
                                                     &context->config );
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
     DisplayLayerFuncs          *funcs;

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
     build_updated_config( context, config, &region_config, NULL );

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
          }

          *ret_failed = flags;
     }

     return ret;
}

DFBResult
dfb_layer_context_set_configuration( CoreLayerContext            *context,
                                     const DFBDisplayLayerConfig *config )
{
     DFBResult                   ret;
     CoreLayer                  *layer;
     CoreLayerShared            *shared;
     CoreLayerRegionConfig       region_config;
     CoreLayerRegionConfigFlags  flags;
     DisplayLayerFuncs          *funcs;

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
     build_updated_config( context, config, &region_config, &flags );

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
               bool surface = (shared->description.caps & DLCAPS_SURFACE);

               if (shared->description.caps & DLCAPS_SOURCES)
                    surface = (region_config.source_id == DLSID_SURFACE);

               /* (Re)allocate the region's surface. */
               if (surface) {
                    flags |= CLRCF_SURFACE | CLRCF_PALETTE;

                    if (D_FLAGS_IS_SET( region->state, CLRSF_ENABLED ))
                         ret = reallocate_surface( layer, region, &region_config,
                                                   &context->config );
                    else
                         ret = allocate_surface( layer, region, &region_config );

                    if (ret) {
                         dfb_layer_region_unlock( region );
                         dfb_layer_region_unref( region );
                         dfb_layer_context_unlock( context );
                         return ret;
                    }
               }
               else if (region->surface)
                    deallocate_surface( layer, region );

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

          /* Update the window stack. */
          if (context->stack) {
               CoreWindowStack *stack = context->stack;

               /* Update hardware flag. */
               stack->hw_mode = (region_config.buffermode == DLBM_WINDOWS);

               /* Tell the windowing core about the new size. */
               dfb_windowstack_resize( stack,
                                       region_config.width,
                                       region_config.height );

               /* FIXME: call only if really needed */
               dfb_windowstack_repaint_all( stack );
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
          ret = dfb_layer_region_set_configuration( context->primary.region,
                                                    config, flags );
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
                                    __u8 r, __u8 g, __u8 b )
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
     config.src_key.r = r;
     config.src_key.g = g;
     config.src_key.b = b;

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_SRCKEY );

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_dst_colorkey( CoreLayerContext *context,
                                    __u8 r, __u8 g, __u8 b )
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
     if (DFB_LOCATION_EQUAL( context->screen, *location )) {
          dfb_layer_context_unlock( context );
          return DFB_OK;
     }

     /* Take the current configuration. */
     config = context->primary.config;

     /* Calculate new absolute screen coordinates. */
     screen_rectangle( context, location, &config.dest );

     /* Try to set the new configuration. */
     ret = update_primary_region_config( context, &config, CLRCF_DEST );
     if (ret == DFB_OK)
          context->screen = *location;

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
}

DFBResult
dfb_layer_context_set_opacity( CoreLayerContext *context,
                               __u8              opacity )
{
     DFBResult             ret;
     CoreLayerRegionConfig config;

     D_ASSERT( context != NULL );

     /* Lock the context. */
     if (dfb_layer_context_lock( context ))
          return DFB_FUSION;

     /* Do nothing if the opacity didn't change. */
     if (context->opacity == opacity) {
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
          context->opacity = opacity;

     /* Unlock the context. */
     dfb_layer_context_unlock( context );

     return ret;
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
dfb_layer_context_create_window( CoreLayerContext        *context,
                                 int                      x,
                                 int                      y,
                                 int                      width,
                                 int                      height,
                                 DFBWindowCapabilities    caps,
                                 DFBSurfaceCapabilities   surface_caps,
                                 DFBSurfacePixelFormat    pixelformat,
                                 CoreWindow             **ret_window )
{
     DFBResult        ret;
     CoreWindow      *window;
     CoreWindowStack *stack;
     CoreLayer       *layer;

     D_ASSERT( context != NULL );
     D_ASSERT( context->stack != NULL );
     D_ASSERT( ret_window != NULL );

     layer = dfb_layer_at( context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );

     if (dfb_layer_context_lock( context ))
         return DFB_FUSION;

     stack = context->stack;

     if (!stack->cursor.set) {
          ret = dfb_windowstack_cursor_enable( stack, true );
          if (ret) {
               dfb_layer_context_unlock( context );
               return ret;
          }
     }

     ret = dfb_window_create( stack, x, y, width, height, caps,
                              surface_caps, pixelformat, &window );
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

     /* Initialize values from layer config. */
     config->width      = context->config.width;
     config->height     = context->config.height;
     config->format     = context->config.pixelformat;
     config->buffermode = context->config.buffermode;
     config->options    = context->config.options;
     config->source_id  = context->config.source;

     /* Initialize source rectangle. */
     config->source.x   = 0;
     config->source.y   = 0;
     config->source.w   = config->width;
     config->source.h   = config->height;

     /* Initialize screen rectangle. */
     screen_rectangle( context, &context->screen, &config->dest );

     /* Set default opacity. */
     config->opacity = 0xff;
}

static void
build_updated_config( CoreLayerContext            *context,
                      const DFBDisplayLayerConfig *update,
                      CoreLayerRegionConfig       *ret_config,
                      CoreLayerRegionConfigFlags  *ret_flags )
{
     CoreLayerRegionConfigFlags flags = CLRCF_NONE;

     D_ASSERT( context != NULL );
     D_ASSERT( update != NULL );
     D_ASSERT( ret_config != NULL );

     /* Get the current region configuration. */
     *ret_config = context->primary.config;

     /* Reset source rectangle. */
     if (update->flags & (DLCONF_WIDTH | DLCONF_HEIGHT)) {
          flags |= CLRCF_SOURCE;
          ret_config->source.x = 0;
          ret_config->source.y = 0;
          ret_config->source.w = ret_config->width;
          ret_config->source.h = ret_config->height;
     }

     /* Change width. */
     if (update->flags & DLCONF_WIDTH) {
          flags |= CLRCF_WIDTH;
          ret_config->width    = update->width;
          ret_config->source.w = update->width;
     }

     /* Change height. */
     if (update->flags & DLCONF_HEIGHT) {
          flags |= CLRCF_HEIGHT;
          ret_config->height   = update->height;
          ret_config->source.h = update->height;
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
     DFBResult               ret;
     DisplayLayerFuncs      *funcs;
     CoreSurface            *surface = NULL;
     DFBSurfaceCapabilities  caps    = DSCAPS_VIDEOONLY;

     D_ASSERT( layer != NULL );
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
          /* Choose surface capabilities depending on the buffer mode. */
          switch (config->buffermode) {
               case DLBM_FRONTONLY:
                    break;

               case DLBM_TRIPLE:
                    caps |= DSCAPS_TRIPLE;
                    break;

               case DLBM_BACKVIDEO:
                    caps |= DSCAPS_DOUBLE;
                    break;

               case DLBM_BACKSYSTEM:
                    D_ONCE("DLBM_BACKSYSTEM as default is unimplemented");
                    break;

               default:
                    D_BUG("unknown buffermode");
                    break;
          }

          if (config->options & DLOP_DEINTERLACING)
               caps |= DSCAPS_INTERLACED;

          /* Use the default surface creation. */
          ret = dfb_surface_create( layer->core,
                                    config->width, config->height,
                                    config->format, CSP_VIDEOONLY,
                                    caps, NULL, &surface );
          if (ret) {
               D_ERROR( "DirectFB/core/layers: Surface creation failed!\n" );
               return ret;
          }
     }

     /* Tell the region about it's new surface (adds a global reference). */
     ret = dfb_layer_region_set_surface( region, surface );

     /* Remove local reference of dfb_surface_create(). */
     dfb_surface_unref( surface );

     return ret;
}

static DFBResult
reallocate_surface( CoreLayer             *layer,
                    CoreLayerRegion       *region,
                    CoreLayerRegionConfig *config,
                    DFBDisplayLayerConfig *previous )
{
     DFBResult          ret;
     DisplayLayerFuncs *funcs;
     CoreSurface       *surface;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( region != NULL );
     D_ASSERT( region->surface != NULL );
     D_ASSERT( config != NULL );
     D_ASSERT( config->buffermode != DLBM_WINDOWS );
     D_ASSERT( previous != NULL );
     D_ASSERT( previous->buffermode != DLBM_WINDOWS );

     funcs   = layer->funcs;
     surface = region->surface;

     if (funcs->ReallocateSurface)
          return funcs->ReallocateSurface( layer, layer->driver_data,
                                           layer->layer_data,
                                           region->region_data,
                                           config, surface );

     /* FIXME: write surface management functions
               for easier configuration changes */

     if (previous->buffermode != config->buffermode) {
          DFBSurfaceCapabilities old_caps = surface->caps;

          switch (config->buffermode) {
               case DLBM_TRIPLE:
                    surface->caps |=  DSCAPS_TRIPLE;
                    surface->caps &= ~DSCAPS_DOUBLE;
                    ret = dfb_surface_reconfig( surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;
               case DLBM_BACKVIDEO:
                    surface->caps |=  DSCAPS_DOUBLE;
                    surface->caps &= ~DSCAPS_TRIPLE;
                    ret = dfb_surface_reconfig( surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;
               case DLBM_BACKSYSTEM:
                    surface->caps |=  DSCAPS_DOUBLE;
                    surface->caps &= ~DSCAPS_TRIPLE;
                    ret = dfb_surface_reconfig( surface,
                                                CSP_VIDEOONLY, CSP_SYSTEMONLY );
                    break;
               case DLBM_FRONTONLY:
                    surface->caps &= ~DSCAPS_FLIPPING;
                    ret = dfb_surface_reconfig( surface,
                                                CSP_VIDEOONLY, CSP_VIDEOONLY );
                    break;

               default:
                    D_BUG("unknown buffermode");
                    return DFB_BUG;
          }

          if (ret) {
               surface->caps = old_caps;
               return ret;
          }
     }

     if (config->width  != previous->width  ||
         config->height != previous->height ||
         config->format != previous->pixelformat)
     {
          ret = dfb_surface_reformat( layer->core, surface, config->width,
                                      config->height, config->format );
          if (ret)
               return ret;
     }

     if (config->options & DLOP_DEINTERLACING)
          surface->caps |= DSCAPS_INTERLACED;
     else
          surface->caps &= ~DSCAPS_INTERLACED;

     return DFB_OK;
}

static DFBResult
deallocate_surface( CoreLayer *layer, CoreLayerRegion *region )
{
     DFBResult          ret;
     DisplayLayerFuncs *funcs;
     CoreSurface       *surface;

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

     ret = dfb_screen_get_screen_size( layer->screen, &width, &height );
     if (ret) {
          D_WARN( "could not determine screen size" );

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

