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

#include <direct/messages.h>

#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/layers_internal.h>
#include <core/surfaces.h>

#include <gfx/util.h>

#include <direct/util.h>


static DFBResult set_region      ( CoreLayerRegion            *region,
                                   CoreLayerRegionConfig      *config,
                                   CoreLayerRegionConfigFlags  flags,
                                   CoreSurface                *surface );

static DFBResult realize_region  ( CoreLayerRegion            *region );

static DFBResult unrealize_region( CoreLayerRegion            *region );

/******************************************************************************/

static void
region_destructor( FusionObject *object, bool zombie )
{
     CoreLayerRegion  *region  = (CoreLayerRegion*) object;
     CoreLayerContext *context = region->context;
     CoreLayer        *layer   = dfb_layer_at( context->layer_id );
     CoreLayerShared  *shared  = layer->shared;

     (void) shared;

     D_DEBUG( "DirectFB/core/layers: destroying region %p (%s, %dx%d, "
               "%s, %s, %s, %s%s)\n", region, shared->description.name,
               region->config.width, region->config.height,
               D_FLAGS_IS_SET( region->state,
                            CLRSF_CONFIGURED ) ? "configured" : "unconfigured",
               D_FLAGS_IS_SET( region->state,
                            CLRSF_ENABLED ) ? "enabled" : "disabled",
               D_FLAGS_IS_SET( region->state,
                            CLRSF_ACTIVE ) ? "active" : "inactive",
               D_FLAGS_IS_SET( region->state,
                            CLRSF_REALIZED ) ? "realized" : "not realized",
               zombie ? " - ZOMBIE" : "");

     /* Hide region etc. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_ENABLED ))
          dfb_layer_region_disable( region );

     /* Remove the region from the context. */
     dfb_layer_context_remove_region( region->context, region );

     /* Throw away its surface. */
     if (region->surface) {
          /* Detach the global listener. */
          dfb_surface_detach_global( region->surface,
                                     &region->surface_reaction );

          /* Unlink from structure. */
          dfb_surface_unlink( &region->surface );
     }

     /* Unlink the context from the structure. */
     dfb_layer_context_unlink( &region->context );

     /* Deinitialize the lock. */
     fusion_skirmish_destroy( &region->lock );

     /* Free driver's region data. */
     if (region->region_data)
          SHFREE( region->region_data );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

/******************************************************************************/

FusionObjectPool *
dfb_layer_region_pool_create()
{
     return fusion_object_pool_create( "Layer Region Pool",
                                       sizeof(CoreLayerRegion),
                                       sizeof(CoreLayerRegionNotification),
                                       region_destructor );
}

/******************************************************************************/

DFBResult
dfb_layer_region_create( CoreLayerContext  *context,
                         CoreLayerRegion  **ret_region )
{
     CoreLayer       *layer;
     CoreLayerRegion *region;

     D_ASSERT( context != NULL );
     D_ASSERT( ret_region != NULL );

     layer = dfb_layer_at( context->layer_id );

     /* Create the object. */
     region = dfb_core_create_layer_region( layer->core );
     if (!region)
          return DFB_FUSION;

     /* Link the context into the structure. */
     if (dfb_layer_context_link( &region->context, context )) {
          fusion_object_destroy( &region->object );
          return DFB_FUSION;
     }

     /* Initialize the lock. */
     if (fusion_skirmish_init( &region->lock )) {
          dfb_layer_context_unlink( &region->context );
          fusion_object_destroy( &region->object );
          return DFB_FUSION;
     }

     /* Activate the object. */
     fusion_object_activate( &region->object );

     /* Add the region to the context. */
     dfb_layer_context_add_region( context, region );

     /* Return the new region. */
     *ret_region = region;

     return DFB_OK;
}

DFBResult
dfb_layer_region_activate( CoreLayerRegion *region )
{
     DFBResult ret;

     D_ASSERT( region != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( ! D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE ) );

     if (D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE )) {
          dfb_layer_region_unlock( region );
          return DFB_OK;
     }

     /* Realize the region if it's enabled. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_ENABLED )) {
          ret = realize_region( region );
          if (ret) {
               dfb_layer_region_unlock( region );
               return ret;
          }
     }

     /* Update the region's state. */
     D_FLAGS_SET( region->state, CLRSF_ACTIVE );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_deactivate( CoreLayerRegion *region )
{
     DFBResult ret;

     D_ASSERT( region != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE ) );

     if (! D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE )) {
          dfb_layer_region_unlock( region );
          return DFB_OK;
     }

     /* Unrealize the region if it's enabled. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_ENABLED )) {
          ret = unrealize_region( region );
          if (ret)
               return ret;
     }

     /* Update the region's state. */
     D_FLAGS_CLEAR( region->state, CLRSF_ACTIVE );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_enable( CoreLayerRegion *region )
{
     DFBResult ret;

     D_ASSERT( region != NULL );
     D_ASSERT( region->context != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( ! D_FLAGS_IS_SET( region->state, CLRSF_ENABLED ) );

     if (D_FLAGS_IS_SET( region->state, CLRSF_ENABLED )) {
          dfb_layer_region_unlock( region );
          return DFB_OK;
     }

     /* Realize the region if it's active. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE )) {
          ret = realize_region( region );
          if (ret) {
               dfb_layer_region_unlock( region );
               return ret;
          }
     }

     /* Update the region's state. */
     D_FLAGS_SET( region->state, CLRSF_ENABLED );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_disable( CoreLayerRegion *region )
{
     DFBResult ret;

     D_ASSERT( region != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( D_FLAGS_IS_SET( region->state, CLRSF_ENABLED ) );

     if (! D_FLAGS_IS_SET( region->state, CLRSF_ENABLED )) {
          dfb_layer_region_unlock( region );
          return DFB_OK;
     }

     /* Unrealize the region if it's active. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE )) {
          ret = unrealize_region( region );
          if (ret)
               return ret;
     }

     /* Update the region's state. */
     D_FLAGS_CLEAR( region->state, CLRSF_ENABLED );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_set_surface( CoreLayerRegion *region,
                              CoreSurface     *surface )
{
     DFBResult ret;

     D_ASSERT( region != NULL );
     D_ASSERT( surface != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     if (region->surface != surface) {
          /* Setup hardware for the new surface if the region is realized. */
          if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
               ret = set_region( region, &region->config, CLRCF_SURFACE | CLRCF_PALETTE, surface );
               if (ret) {
                    dfb_layer_region_unlock( region );
                    return ret;
               }
          }

          /* Throw away the old surface. */
          if (region->surface) {
               /* Detach the global listener. */
               dfb_surface_detach_global( region->surface,
                                          &region->surface_reaction );

               /* Unlink surface from structure. */
               dfb_surface_unlink( &region->surface );
          }

          /* Take the new surface. */
          if (surface) {
               /* Link surface into structure. */
               if (dfb_surface_link( &region->surface, surface )) {
                    D_WARN( "region lost it's surface" );
                    dfb_layer_region_unlock( region );
                    return DFB_FUSION;
               }

               /* Attach the global listener. */
               dfb_surface_attach_global( region->surface,
                                          DFB_LAYER_REGION_SURFACE_LISTENER,
                                          region, &region->surface_reaction );
          }
     }

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_get_surface( CoreLayerRegion  *region,
                              CoreSurface     **ret_surface )
{
     D_ASSERT( region != NULL );
     D_ASSERT( ret_surface != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( region->surface != NULL );

     /* Check for NULL surface. */
     if (!region->surface) {
          dfb_layer_region_unlock( region );
          return DFB_UNSUPPORTED;
     }

     /* Increase the surface's reference counter. */
     if (dfb_surface_ref( region->surface )) {
          dfb_layer_region_unlock( region );
          return DFB_FUSION;
     }

     /* Return the surface. */
     *ret_surface = region->surface;

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_flip_update( CoreLayerRegion     *region,
                              const DFBRegion     *update,
                              DFBSurfaceFlipFlags  flags )
{
     DFBResult          ret = DFB_OK;
     CoreLayer         *layer;
     CoreLayerContext  *context;
     CoreSurface       *surface;
     SurfaceBuffer     *buffer;
     DisplayLayerFuncs *funcs;

     D_ASSERT( region != NULL );
     D_ASSERT( region->context != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( region->surface != NULL );

     /* Check for NULL surface. */
     if (!region->surface) {
          dfb_layer_region_unlock( region );
          return DFB_UNSUPPORTED;
     }

     context = region->context;
     surface = region->surface;
     buffer  = surface->back_buffer;
     layer   = dfb_layer_at( context->layer_id );

     D_ASSERT( layer->funcs != NULL );

     funcs = layer->funcs;

     /* Depending on the buffer mode... */
     switch (region->config.buffermode) {
          case DLBM_TRIPLE:
          case DLBM_BACKVIDEO:
               /* Check if simply swapping the buffers is possible... */
               if (!(flags & DSFLIP_BLIT) &&
                   (!update || (update->x1 == 0 &&
                                update->y1 == 0 &&
                                update->x2 == surface->width - 1 &&
                                update->y2 == surface->height - 1)))
               {
                    /* Use the driver's routine if the region is realized. */
                    if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
                         D_ASSUME( funcs->FlipRegion != NULL );

                         if (buffer->video.access & VAF_HARDWARE_WRITE) {
                              dfb_gfxcard_wait_serial( &buffer->video.serial );

                              buffer->video.access &= ~VAF_HARDWARE_WRITE;
                         }

                         if (funcs->FlipRegion)
                              ret = funcs->FlipRegion( layer,
                                                       layer->driver_data,
                                                       layer->layer_data,
                                                       region->region_data,
                                                       surface, flags );
                    }
                    else
                         /* Just do the hardware independent work. */
                         dfb_surface_flip_buffers( surface, false );

                    break;
               }

               /* fall through */

          case DLBM_BACKSYSTEM:
               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC)
                    dfb_layer_wait_vsync( layer );

               /* ...or copy updated contents from back to front buffer. */
               dfb_back_to_front_copy( surface, update );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAIT)
                    dfb_layer_wait_vsync( layer );

               /* fall through */

          case DLBM_FRONTONLY:
               /* Tell the driver about the update if the region is realized. */
               if (funcs->UpdateRegion &&
                   D_FLAGS_IS_SET( region->state, CLRSF_REALIZED ))
               {
                    ret = funcs->UpdateRegion( layer,
                                               layer->driver_data,
                                               layer->layer_data,
                                               region->region_data,
                                               surface, update );
               }
               break;

          default:
               D_BUG("unknown buffer mode");
               ret = DFB_BUG;
     }

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return ret;
}

DFBResult
dfb_layer_region_set_configuration( CoreLayerRegion            *region,
                                    CoreLayerRegionConfig      *config,
                                    CoreLayerRegionConfigFlags  flags )
{
     DFBResult              ret;
     CoreLayer             *layer;
     DisplayLayerFuncs     *funcs;
     CoreLayerRegionConfig  new_config;

     D_ASSERT( region != NULL );
     D_ASSERT( region->context != NULL );
     D_ASSERT( config != NULL );
     D_ASSERT( config->buffermode != DLBM_WINDOWS );
     D_ASSERT( (flags == CLRCF_ALL) || (region->state & CLRSF_CONFIGURED) );

     D_ASSUME( flags != CLRCF_NONE );
     D_ASSUME( ! (flags & ~CLRCF_ALL) );

     layer = dfb_layer_at( region->context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( layer->funcs->TestRegion != NULL );

     funcs = layer->funcs;

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     /* Full configuration supplied? */
     if (flags == CLRCF_ALL) {
          new_config = *config;
     }
     else {
          /* Use the current configuration. */
          new_config = region->config;

          /* Update each modified entry. */
          if (flags & CLRCF_WIDTH)
               new_config.width = config->width;

          if (flags & CLRCF_HEIGHT)
               new_config.height = config->height;

          if (flags & CLRCF_FORMAT)
               new_config.format = config->format;

          if (flags & CLRCF_BUFFERMODE)
               new_config.buffermode = config->buffermode;

          if (flags & CLRCF_OPTIONS)
               new_config.options = config->options;

          if (flags & CLRCF_SOURCE)
               new_config.source = config->source;

          if (flags & CLRCF_DEST)
               new_config.dest = config->dest;

          if (flags & CLRCF_OPACITY)
               new_config.opacity = config->opacity;

          if (flags & CLRCF_SRCKEY)
               new_config.src_key = config->src_key;

          if (flags & CLRCF_DSTKEY)
               new_config.dst_key = config->dst_key;

          if (flags & CLRCF_PARITY)
               new_config.parity = config->parity;
     }

     /* Check if the new configuration is supported. */
     ret = funcs->TestRegion( layer, layer->driver_data, layer->layer_data,
                              &new_config, NULL );
     if (ret) {
          dfb_layer_region_unlock( region );
          return ret;
     }

     /* Propagate new configuration to the driver if the region is realized. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
          ret = set_region( region, &new_config, flags, region->surface );
          if (ret) {
               dfb_layer_region_unlock( region );
               return ret;
          }
     }

     /* Update the region's current configuration. */
     region->config = new_config;

     /* Update the region's state. */
     D_FLAGS_SET( region->state, CLRSF_CONFIGURED );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_get_configuration( CoreLayerRegion       *region,
                                    CoreLayerRegionConfig *config )
{
     D_ASSERT( region != NULL );
     D_ASSERT( config != NULL );

     D_ASSERT( D_FLAGS_IS_SET( region->state, CLRSF_CONFIGURED ) );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     /* Return the current configuration. */
     *config = region->config;

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DirectResult
dfb_layer_region_lock( CoreLayerRegion *region )
{
     D_ASSERT( region != NULL );

     return fusion_skirmish_prevail( &region->lock );
}

DirectResult
dfb_layer_region_unlock( CoreLayerRegion *region )
{
     D_ASSERT( region != NULL );

     return fusion_skirmish_dismiss( &region->lock );
}

/******************************************************************************/

/*
 * listen to the layer's surface
 */
ReactionResult
_dfb_layer_region_surface_listener( const void *msg_data, void *ctx )
{
     CoreSurfaceNotificationFlags   flags;
     CoreSurface                   *surface;
     CoreLayer                     *layer;
     CoreLayerShared               *shared;
     DisplayLayerFuncs             *funcs;
     const CoreSurfaceNotification *notification = msg_data;
     CoreLayerRegion               *region       = ctx;

     D_ASSERT( notification != NULL );
     D_ASSERT( region != NULL );
     D_ASSERT( region->context != NULL );

     D_ASSERT( notification->surface != NULL );

     D_ASSUME( notification->surface == region->surface );

     if (notification->surface != region->surface)
          return RS_OK;

     layer = dfb_layer_at( region->context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( layer->funcs->SetRegion != NULL );
     D_ASSERT( layer->shared != NULL );

     funcs   = layer->funcs;
     shared  = layer->shared;

     flags   = notification->flags;
     surface = notification->surface;

     if (flags & CSNF_DESTROY) {
          D_WARN( "layer region surface destroyed" );
          region->surface = NULL;
          return RS_REMOVE;
     }

     if (dfb_layer_region_lock( region ))
          return RS_OK;

     if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
          if (flags & (CSNF_PALETTE_CHANGE | CSNF_PALETTE_UPDATE)) {
               if (surface->palette)
                    funcs->SetRegion( layer,
                                      layer->driver_data, layer->layer_data,
                                      region->region_data, &region->config,
                                      CLRCF_PALETTE, surface, surface->palette );
          }

          if ((flags & CSNF_FIELD) && funcs->SetInputField)
               funcs->SetInputField( layer,
                                     layer->driver_data, layer->layer_data,
                                     region->region_data, surface->field );
     }

     dfb_layer_region_unlock( region );

     return RS_OK;
}

/******************************************************************************/

static DFBResult
set_region( CoreLayerRegion            *region,
            CoreLayerRegionConfig      *config,
            CoreLayerRegionConfigFlags  flags,
            CoreSurface                *surface )
{
     CoreLayer         *layer;
     DisplayLayerFuncs *funcs;

     D_ASSERT( region != NULL );
     D_ASSERT( region->context != NULL );
     D_ASSERT( config != NULL );
     D_ASSERT( config->buffermode != DLBM_WINDOWS );

     D_ASSERT( D_FLAGS_IS_SET( region->state, CLRSF_REALIZED ) );

     layer = dfb_layer_at( region->context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( layer->funcs->SetRegion != NULL );

     funcs = layer->funcs;

     /* Setup hardware. */
     return funcs->SetRegion( layer, layer->driver_data, layer->layer_data,
                              region->region_data, config, flags,
                              surface, surface ? surface->palette : NULL );
}

static DFBResult
realize_region( CoreLayerRegion *region )
{
     DFBResult          ret;
     CoreLayer         *layer;
     DisplayLayerFuncs *funcs;

     D_ASSERT( region != NULL );
     D_ASSERT( region->context != NULL );
     D_ASSERT( D_FLAGS_IS_SET( region->state, CLRSF_CONFIGURED ) );
     D_ASSERT( ! D_FLAGS_IS_SET( region->state, CLRSF_REALIZED ) );

     layer = dfb_layer_at( region->context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );

     funcs = layer->funcs;

     /* Allocate the driver's region data. */
     if (funcs->RegionDataSize) {
          int size = funcs->RegionDataSize();

          if (size > 0) {
               region->region_data = SHCALLOC( 1, size );
               if (!region->region_data)
                    return DFB_NOSYSTEMMEMORY;
          }
     }

     /* Add the region to the driver. */
     if (funcs->AddRegion) {
          ret = funcs->AddRegion( layer,
                                  layer->driver_data, layer->layer_data,
                                  region->region_data, &region->config );
          if (ret) {
               if (region->region_data) {
                    SHFREE( region->region_data );
                    region->region_data = NULL;
               }

               return ret;
          }
     }

     /* Update the region's state. */
     D_FLAGS_SET( region->state, CLRSF_REALIZED );

     /* Initially setup hardware. */
     ret = set_region( region, &region->config, CLRCF_ALL, region->surface );
     if (ret) {
          unrealize_region( region );
          return ret;
     }

     return DFB_OK;
}

static DFBResult
unrealize_region( CoreLayerRegion *region )
{
     DFBResult          ret;
     CoreLayer         *layer;
     DisplayLayerFuncs *funcs;

     D_ASSERT( region != NULL );
     D_ASSERT( region->context != NULL );
     D_ASSERT( D_FLAGS_IS_SET( region->state, CLRSF_REALIZED ) );

     layer = dfb_layer_at( region->context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );

     funcs = layer->funcs;

     /* Remove the region from hardware and driver. */
     if (funcs->RemoveRegion) {
          ret = funcs->RemoveRegion( layer, layer->driver_data,
                                     layer->layer_data, region->region_data );
          if (ret) {
               D_ERROR( "DirectFB/core/layers: Could not remove region!"
                         " (%s)\n", DirectFBErrorString(ret) );
               return ret;
          }
     }

     /* Deallocate the driver's region data. */
     if (region->region_data) {
          SHFREE( region->region_data );
          region->region_data = NULL;
     }

     /* Update the region's state. */
     D_FLAGS_CLEAR( region->state, CLRSF_REALIZED );

     return DFB_OK;
}

