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

#ifndef __CORE__LAYER_CONTEXT_H__
#define __CORE__LAYER_CONTEXT_H__

#include <directfb.h>

#include <core/coretypes.h>
#include <fusion/object.h>

typedef enum {
     CLCNF_ACTIVATED   = 0x00000001,
     CLCNF_DEACTIVATED = 0x00000002
} CoreLayerContextNotificationFlags;

typedef struct {
     CoreLayerContextNotificationFlags  flags;
     CoreLayerContext                  *context;
} CoreLayerContextNotification;

/*
 * Creates a pool of layer context objects.
 */
FusionObjectPool *dfb_layer_context_pool_create( const FusionWorld *world );

/*
 * Generates dfb_layer_context_ref(), dfb_layer_context_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreLayerContext, dfb_layer_context )


DFBResult dfb_layer_context_init( CoreLayerContext *context,
                                  CoreLayer        *layer,
                                  bool              stack );

/*
 * Locking
 */
DirectResult dfb_layer_context_lock  ( CoreLayerContext *context );
DirectResult dfb_layer_context_unlock( CoreLayerContext *context );


CoreWindowStack *dfb_layer_context_windowstack( const CoreLayerContext *context );

bool             dfb_layer_context_active     ( const CoreLayerContext *context );







DFBResult dfb_layer_context_get_primary_region( CoreLayerContext  *context,
                                                bool               create,
                                                CoreLayerRegion  **ret_region );

/*
 * configuration testing/setting/getting
 */
DFBResult dfb_layer_context_test_configuration ( CoreLayerContext            *context,
                                                 const DFBDisplayLayerConfig *config,
                                                 DFBDisplayLayerConfigFlags  *ret_failed );

DFBResult dfb_layer_context_set_configuration  ( CoreLayerContext            *context,
                                                 const DFBDisplayLayerConfig *config );

DFBResult dfb_layer_context_get_configuration  ( CoreLayerContext            *context,
                                                 DFBDisplayLayerConfig       *ret_config );


/*
 * configuration details
 */
DFBResult dfb_layer_context_set_src_colorkey   ( CoreLayerContext            *context,
                                                 u8                           r,
                                                 u8                           g,
                                                 u8                           b,
                                                 int                          index );

DFBResult dfb_layer_context_set_dst_colorkey   ( CoreLayerContext            *context,
                                                 u8                           r,
                                                 u8                           g,
                                                 u8                           b,
                                                 int                          index );

DFBResult dfb_layer_context_set_sourcerectangle( CoreLayerContext            *context,
                                                 const DFBRectangle          *source );

DFBResult dfb_layer_context_set_screenlocation ( CoreLayerContext            *context,
                                                 const DFBLocation           *location );

DFBResult dfb_layer_context_set_screenrectangle( CoreLayerContext            *context,
                                                 const DFBRectangle          *rectangle );

DFBResult dfb_layer_context_set_screenposition ( CoreLayerContext            *context,
                                                 int                          x,
                                                 int                          y );

DFBResult dfb_layer_context_set_opacity        ( CoreLayerContext            *context,
                                                 u8                           opacity );

DFBResult dfb_layer_context_set_rotation       ( CoreLayerContext            *context,
                                                 int                          rotation );

DFBResult dfb_layer_context_set_coloradjustment( CoreLayerContext            *context,
                                                 const DFBColorAdjustment    *adjustment );

DFBResult dfb_layer_context_get_coloradjustment( CoreLayerContext            *context,
                                                 DFBColorAdjustment          *ret_adjustment );

DFBResult dfb_layer_context_set_stereo_depth   ( CoreLayerContext            *context,
                                                 bool                         follow_video,
                                                 int                          z );

DFBResult dfb_layer_context_get_stereo_depth   ( CoreLayerContext            *context,
                                                 bool                        *follow_video,
                                                 int                         *ret_z );

DFBResult dfb_layer_context_set_field_parity   ( CoreLayerContext            *context,
                                                 int                          field );

DFBResult dfb_layer_context_set_clip_regions   ( CoreLayerContext            *context,
                                                 const DFBRegion             *regions,
                                                 int                          num_regions,
                                                 DFBBoolean                   positive );


/*
 * window control
 */
DFBResult dfb_layer_context_create_window( CoreDFB                     *core,
                                           CoreLayerContext            *context,
                                           const DFBWindowDescription  *desc,
                                           CoreWindow                 **ret_window );

CoreWindow      *dfb_layer_context_find_window( CoreLayerContext       *context,
                                                DFBWindowID             id );




DFBResult dfb_layer_context_allocate_surface    ( CoreLayer                   *layer,
                                                  CoreLayerRegion             *region,
                                                  CoreLayerRegionConfig       *config );

DFBResult dfb_layer_context_reallocate_surface  ( CoreLayer                   *layer,
                                                  CoreLayerRegion             *region,
                                                  CoreLayerRegionConfig       *config );

DFBResult dfb_layer_context_deallocate_surface  ( CoreLayer                   *layer,
                                                  CoreLayerRegion             *region );

#endif

