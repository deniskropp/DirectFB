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

#ifndef __CORE__LAYERS_INTERNAL_H__
#define __CORE__LAYERS_INTERNAL_H__

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/fusion/object.h>
#include <core/fusion/property.h>
#include <core/fusion/vector.h>

#include <core/layers.h>
#include <core/layer_region.h>
#include <core/state.h>

typedef struct {
     FusionVector      stack;
     int               active;

     CoreLayerContext *primary;
} CoreLayerContexts;

typedef struct {
     DFBDisplayLayerID           layer_id;

     DFBDisplayLayerDescription  description;
     DFBDisplayLayerConfig       default_config;
     DFBColorAdjustment          default_adjustment;

     void                       *layer_data;

     FusionSkirmish              lock;

     CoreLayerContexts           contexts;

     bool                        suspended;
} CoreLayerShared;

struct __DFB_CoreLayer {
     CoreLayerShared   *shared;

     CoreDFB           *core;

     GraphicsDevice    *device;

     void              *driver_data;
     void              *layer_data;   /* copy of shared->layer_data */

     DisplayLayerFuncs *funcs;

     CardState          state;
};

struct __DFB_CoreLayerContext {
     FusionObject             object;

     DFBDisplayLayerID        layer_id;

     FusionSkirmish           lock;

     bool                     active;

     FusionVector             regions;

     DFBDisplayLayerConfig    config;        /* current configuration */

     CoreLayerRegion         *primary;       /* region used for buffer modes
                                                other than DLBM_WINDOWS */

     __u8                     opacity;       /* if enabled this value controls
                                                blending of the whole layer */

     DFBLocation              screen;        /* normalized screen coordinates */

     DFBColorAdjustment       adjustment;

     CoreWindowStack         *stack;         /* every layer has its own
                                                windowstack as every layer has
                                                its own pixel buffer */
};

typedef enum {
     CLRSF_NONE       = 0x00000000,

     CLRSF_CONFIGURED = 0x00000001,
     CLRSF_ENABLED    = 0x00000002,
     CLRSF_ACTIVE     = 0x00000004,
     CLRSF_REALIZED   = 0x00000008,

     CLRSF_ALL        = 0x0000000F
} CoreLayerRegionStateFlags;

struct __DFB_CoreLayerRegion {
     FusionObject                object;

     CoreLayerContext           *context;

     FusionSkirmish              lock;

     CoreLayerRegionStateFlags   state;

     CoreLayerRegionConfig       config;

     CoreSurface                *surface;
     GlobalReaction              surface_reaction;

     void                       *region_data;
};


/* Called at the end of dfb_layer_region_create(). */
DFBResult dfb_layer_context_add_region( CoreLayerContext *context,
                                        CoreLayerRegion  *region );

/* Called early in the region_destructor(). */
DFBResult dfb_layer_context_remove_region( CoreLayerContext *context,
                                           CoreLayerRegion  *region );


/* global reactions */
ReactionResult _dfb_layer_region_surface_listener( const void *msg_data,
                                                   void       *ctx );

#endif

