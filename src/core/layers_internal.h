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

#ifndef __CORE__LAYERS_INTERNAL_H__
#define __CORE__LAYERS_INTERNAL_H__

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <fusion/object.h>
#include <fusion/property.h>
#include <fusion/vector.h>

#include <core/layers.h>
#include <core/layer_region.h>
#include <core/state.h>

typedef struct {
     FusionVector      stack;
     int               active;

     CoreLayerContext *primary;
} CoreLayerContexts;

typedef struct {
     int                                index;
     DFBDisplayLayerSourceDescription   description;
} CoreLayerSource;

typedef struct {
     DFBDisplayLayerID                  layer_id;

     DFBDisplayLayerDescription         description;
     DFBDisplayLayerConfig              default_config;
     DFBColorAdjustment                 default_adjustment;

     CoreLayerSource                   *sources;

     void                              *layer_data;

     FusionSkirmish                     lock;

     CoreLayerContexts                  contexts;

     bool                               suspended;

     FusionVector                       added_regions;

     FusionSHMPoolShared               *shmpool;

     FusionCall                         call;

     DFBSurfacePixelFormat              pixelformat;
} CoreLayerShared;

struct __DFB_CoreLayer {
     CoreLayerShared         *shared;

     CoreDFB                 *core;

     CoreGraphicsDevice      *device;

     CoreScreen              *screen;

     void                    *driver_data;
     void                    *layer_data;   /* copy of shared->layer_data */

     const DisplayLayerFuncs *funcs;

     CardState                state;
};

typedef enum {
     CLLM_LOCATION,      /* Keep normalized area. */
     CLLM_CENTER,        /* Center layer after resizing destination area. */
     CLLM_POSITION,      /* Keep pixel position, but resize area. */
     CLLM_RECTANGLE      /* Keep pixel based area. */
} CoreLayerLayoutMode;

struct __DFB_CoreLayerContext {
     FusionObject                object;

     int                         magic;

     DFBDisplayLayerID           layer_id;

     FusionSkirmish              lock;

     bool                        active;     /* Is this the active context? */

     DFBDisplayLayerConfig       config;     /* Current layer configuration. */
     int                         rotation;

     FusionVector                regions;    /* All regions created within
                                                this context. */

     struct {
          CoreLayerRegion       *region;     /* Region of layer config if buffer
                                                mode is not DLBM_WINDOWS. */
          CoreLayerRegionConfig  config;     /* Region config used to implement
                                                layer config and settings. */
     } primary;

     struct {
          DFBLocation            location;   /* Normalized screen location. */
          DFBRectangle           rectangle;  /* Pixel based position and size. */

          CoreLayerLayoutMode    mode;       /* ...and how resizing influences them. */
     } screen;

     DFBColorAdjustment          adjustment; /* Color adjustment of the layer.*/

     bool                        follow_video;    /* Stereo ofset is deteremined by video metadata. */
     int                         z;               /* Stereo offset to use when the layer is mixed. */

     CoreWindowStack            *stack;      /* Every layer has its own
                                                windowstack as every layer has
                                                its own pixel buffer. */

     FusionSHMPoolShared        *shmpool;

     FusionCall                  call;
};

typedef enum {
     CLRSF_NONE       = 0x00000000,

     CLRSF_CONFIGURED = 0x00000001,
     CLRSF_ENABLED    = 0x00000002,
     CLRSF_ACTIVE     = 0x00000004,
     CLRSF_REALIZED   = 0x00000008,

     CLRSF_FROZEN     = 0x00000010,

     CLRSF_ALL        = 0x0000001F
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

     CoreSurfaceAccessorID       surface_accessor;

     FusionCall                  call;
};


/* Called at the end of dfb_layer_region_create(). */
DFBResult dfb_layer_context_add_region( CoreLayerContext *context,
                                        CoreLayerRegion  *region );

/* Called early in the region_destructor(). */
DFBResult dfb_layer_context_remove_region( CoreLayerContext *context,
                                           CoreLayerRegion  *region );

/* Called by dfb_layer_activate_context(),
   dfb_layer_remove_context() and dfb_layer_resume(). */
DFBResult dfb_layer_context_activate  ( CoreLayerContext *context );

/* Called by dfb_layer_deactivate_context(),
   dfb_layer_remove_context() and dfb_layer_suspend(). */
DFBResult dfb_layer_context_deactivate( CoreLayerContext *context );

/* global reactions */
ReactionResult _dfb_layer_region_surface_listener( const void *msg_data,
                                                   void       *ctx );

#endif

