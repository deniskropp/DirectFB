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

#include <core/fusion/object.h>
#include <core/fusion/property.h>
#include <core/fusion/vector.h>

#include <core/layers.h>

#include <directfb.h>

typedef struct {
     DFBDisplayLayerID        id;      /* unique id, functions as an index,
                                          primary layer has a fixed id */

     DisplayLayerInfo         layer_info;
     void                    *layer_data;

     FusionObjectPool        *region_pool;

     FusionVector             regions;

     /****/

     DFBDisplayLayerConfig    config;  /* current configuration */

     DFBDisplayLayerConfig    last_config;  /* last 'shared' configuration */

     __u8                     opacity; /* if enabled this value controls
                                          blending of the whole layer */

     /* these are normalized values for stretching layers in hardware */
     struct {
          float     x, y;  /* 0,0 for the primary layer */
          float     w, h;  /* 1,1 for the primary layer */
     } screen;

     DFBColorAdjustment       adjustment;

     /****/

     int                      enabled; /* layers can be turned on and off */

     CoreWindowStack         *stack;   /* every layer has its own
                                          windowstack as every layer has
                                          its own pixel buffer */

     FusionProperty           lock;    /* purchased during exclusive access,
                                          leased during window stack repaint */

     bool                     exclusive; /* helps to detect dead excl. access */

     GlobalReaction           bgimage_reaction;
} CoreLayerShared, DisplayLayerShared;

struct __DFB_CoreLayer {
     DisplayLayerShared *shared;

     GraphicsDevice     *device;

     void               *driver_data;
     void               *layer_data;   /* copy of shared->layer_data */

     DisplayLayerFuncs  *funcs;

     CardState           state;
};

struct __DFB_CoreLayerRegion {
     FusionObject       object;

     FusionSkirmish     lock;

     DFBDisplayLayerID  layer_id;

     DFBRectangle       dst;
     DFBRectangle       src;

     __u8               opacity;

     CoreSurface       *surface;
     GlobalReaction     surface_reaction;
};

ReactionResult _dfb_layer_region_surface_listener( const void *msg_data,
                                                   void       *ctx );

#endif

