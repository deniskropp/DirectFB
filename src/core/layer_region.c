/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2003  convergence GmbH.

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

#include <directfb.h>
#include <core/coretypes.h>
#include <core/coredefs.h>

#include <core/layer_region.h>
#include <core/surfaces.h>

#include <core/layers_internal.h>

static void
region_destructor( FusionObject *object, bool zombie )
{
     CoreLayerRegion *region = (CoreLayerRegion*) object;

     DEBUGMSG("DirectFB/core/layers: destroying region %p (%dx%d%s)\n", region,
              region->src.w, region->src.h, zombie ? " ZOMBIE" : "");

     dfb_layer_disable( dfb_layer_at(region->layer_id) );

     /* Deinitialize the lock. */
     fusion_skirmish_destroy( &region->lock );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

/******************************************************************************/

FusionObjectPool *
dfb_layer_region_pool_create( DFBDisplayLayerID layer_id )
{
     char buf[32];

     snprintf( buf, sizeof(buf), "Region Pool (%d)", layer_id );

     return fusion_object_pool_create( buf,
                                       sizeof(CoreLayerRegion),
                                       sizeof(CoreLayerRegionNotification),
                                       region_destructor );
}

/******************************************************************************/

DFBResult
dfb_layer_region_create( CoreLayer        *layer,
                         CoreLayerRegion **ret_region )
{
     CoreLayerShared *shared;
     CoreLayerRegion *region;

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( layer->shared != NULL );
     DFB_ASSERT( layer->shared->region_pool != NULL );
     DFB_ASSERT( ret_region != NULL );

     shared = layer->shared;

     /* Create the object. */
     region = (CoreLayerRegion*) fusion_object_create( shared->region_pool );
     if (!region)
          return DFB_FUSION;

     /* Initialize the lock. */
     if (fusion_skirmish_init( &region->lock )) {
          fusion_object_destroy( &region->object );
          return DFB_FUSION;
     }

     /* Store layer ID. */
     region->layer_id = shared->id;

     /* Activate the object. */
     fusion_object_activate( &region->object );

     /* Return the new region. */
     *ret_region = region;

     return DFB_OK;
}

DFBResult
dfb_layer_region_set_surface( CoreLayerRegion *region,
                              CoreSurface     *surface )
{
     DFB_ASSERT( region != NULL );

     /* Lock the region. */
     if (fusion_skirmish_prevail( &region->lock ))
          return DFB_FUSION;

     /* Throw away old surface. */
     if (region->surface) {
          /* Detach the global listener. */
          dfb_surface_detach_global( region->surface,
                                     &region->surface_reaction );

          /* Unlink from structure. */
          dfb_surface_unlink( region->surface );

          region->surface = NULL;
     }

     /* Use new surface. */
     if (surface) {
          /* Link surface into structure. */
          if (dfb_surface_link( &region->surface, surface )) {
               fusion_skirmish_dismiss( &region->lock );
               return DFB_FUSION;
          }

          /* Attach the global listener. */
          dfb_surface_attach_global( region->surface,
                                     DFB_LAYER_REGION_SURFACE_LISTENER,
                                     region, &region->surface_reaction );
     }

     /* Unlock the region. */
     fusion_skirmish_dismiss( &region->lock );

     return DFB_OK;
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

     DFB_ASSERT( notification != NULL );
     DFB_ASSERT( region != NULL );

     DFB_ASSERT( notification->surface != NULL );
     DFB_ASSERT( notification->surface == region->surface );

     layer = dfb_layer_at( region->layer_id );

     DFB_ASSERT( layer->funcs != NULL );
     DFB_ASSERT( layer->shared != NULL );

     funcs        = layer->funcs;
     shared       = layer->shared;

     flags        = notification->flags;
     surface      = notification->surface;

     if (flags & CSNF_DESTROY) {
          CAUTION( "layer region surface destroyed" );
          region->surface = NULL;
          return RS_REMOVE;
     }

     if (flags & (CSNF_PALETTE_CHANGE | CSNF_PALETTE_UPDATE)) {
          if (surface->palette && funcs->SetPalette)
               funcs->SetPalette( layer, layer->driver_data,
                                  layer->layer_data, surface->palette );
     }

     if ((flags & CSNF_FIELD) && funcs->SetField)
          funcs->SetField( layer, layer->driver_data,
                           layer->layer_data, surface->field );

     return RS_OK;
}

