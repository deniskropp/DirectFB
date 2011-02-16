/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __CORE__LAYER_REGION_H__
#define __CORE__LAYER_REGION_H__

#include <directfb.h>

#include <core/coretypes.h>
#include <core/layers.h>

#include <fusion/object.h>


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
FusionObjectPool *dfb_layer_region_pool_create( const FusionWorld *world );

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
                                          const DFBRegion      *update,
                                          DFBSurfaceFlipFlags   flags );

DFBResult
dfb_layer_region_flip_update_stereo     ( CoreLayerRegion      *region,
                                          const DFBRegion      *left_update,
                                          const DFBRegion      *right_update,
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
DirectResult dfb_layer_region_lock  ( CoreLayerRegion   *region );
DirectResult dfb_layer_region_unlock( CoreLayerRegion   *region );

#endif

