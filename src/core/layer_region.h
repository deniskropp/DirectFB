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

#ifndef __CORE__LAYER_REGION_H__
#define __CORE__LAYER_REGION_H__

#include <directfb.h>

#include <core/coretypes.h>
#include <core/layers.h>

#include <core/fusion/object.h>


typedef enum {
     CLRNF_NONE        = 0x00000000
} CoreLayerRegionNotificationFlags;

typedef struct {
     CoreLayerRegionNotificationFlags  flags;
     CoreLayerRegion                  *region;
} CoreLayerRegionNotification;

/*
 * Creates a pool of layer region objects.
 */
FusionObjectPool *dfb_layer_region_pool_create();

/*
 * Generates dfb_layer_region_ref(), dfb_layer_region_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreLayerRegion, dfb_layer_region )


DFBResult dfb_layer_region_create       ( CoreLayerContext     *context,
                                          CoreLayerRegion     **ret_region );

DFBResult dfb_layer_region_activate     ( CoreLayerRegion      *region );

DFBResult dfb_layer_region_deactivate   ( CoreLayerRegion      *region );

DFBResult dfb_layer_region_enable       ( CoreLayerRegion      *region );

DFBResult dfb_layer_region_disable      ( CoreLayerRegion      *region );

DFBResult dfb_layer_region_set_surface  ( CoreLayerRegion      *region,
                                          CoreSurface          *surface );

DFBResult dfb_layer_region_get_surface  ( CoreLayerRegion      *region,
                                          CoreSurface         **ret_surface );

DFBResult dfb_layer_region_flip_update  ( CoreLayerRegion      *region,
                                          DFBRegion            *update,
                                          DFBSurfaceFlipFlags   flags );


/*
 * Configuration
 */
DFBResult dfb_layer_region_set_configuration( CoreLayerRegion            *region,
                                              CoreLayerRegionConfig      *config,
                                              CoreLayerRegionConfigFlags  flags );

DFBResult dfb_layer_region_get_configuration( CoreLayerRegion       *region,
                                              CoreLayerRegionConfig *config );


/*
 * Locking
 */
inline FusionResult dfb_layer_region_lock  ( CoreLayerRegion   *region );
inline FusionResult dfb_layer_region_unlock( CoreLayerRegion   *region );

#endif

