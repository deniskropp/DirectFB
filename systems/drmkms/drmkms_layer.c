/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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




//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <core/layers.h>
#include <core/screens.h>

#include <misc/conf.h>
#include <directfb_util.h>


#include "drmkms_system.h"

D_DEBUG_DOMAIN( DRMKMS_Layer, "DRMKMS/Layer", "DRM/KMS Layer" );
D_DEBUG_DOMAIN( DRMKMS_Mode, "DRMKMS/Mode", "DRM/KMS Mode" );

/**********************************************************************************************************************/



static DFBResult
drmkmsInitLayer( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 DFBDisplayLayerDescription *description,
                 DFBDisplayLayerConfig      *config,
                 DFBColorAdjustment         *adjustment )
{
     DRMKMSData *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;
     DRMKMSLayerData  *data   = layer_data;

     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );


     data->index       = shared->layerplane_index_count++;
     data->layer_index = shared->layer_index_count++;
     data->level       = 0;

     description->type             = DLTF_GRAPHICS;
     description->caps             = DLCAPS_SURFACE;
     description->surface_caps     = DSCAPS_NONE;
     description->surface_accessor = CSAID_LAYER0;

     direct_snputs( description->name, "DRMKMS Layer", DFB_DISPLAY_LAYER_DESC_NAME_LENGTH );


     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->width       = dfb_config->mode.width  ?: shared->mode[data->layer_index].hdisplay;
     config->height      = dfb_config->mode.height ?: shared->mode[data->layer_index].vdisplay;

     config->pixelformat = dfb_config->mode.format ?: DSPF_ARGB;
     config->buffermode  = DLBM_FRONTONLY;

     return DFB_OK;
}

static DFBResult
drmkmsTestRegion( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  CoreLayerRegionConfig      *config,
                  CoreLayerRegionConfigFlags *ret_failed )
{

     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;
     DRMKMSLayerData  *data   = layer_data;

     if ((shared->primary_dimension[data->layer_index].w && (shared->primary_dimension[data->layer_index].w > config->width) ) ||
         (shared->primary_dimension[data->layer_index].h && (shared->primary_dimension[data->layer_index].h > config->height ))) {

          D_DEBUG_AT( DRMKMS_Layer, "    -> rejecting layer that is smaller than the screen (drm/kms limitation)\n" );
          if (ret_failed)
               *ret_failed = CLRCF_WIDTH | CLRCF_HEIGHT;

          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
drmkmsSetRegion( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 void                       *region_data,
                 CoreLayerRegionConfig      *config,
                 CoreLayerRegionConfigFlags  updated,
                 CoreSurface                *surface,
                 CorePalette                *palette,
                 CoreSurfaceBufferLock      *left_lock,
                 CoreSurfaceBufferLock      *right_lock )
{
     int               ret;
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;
     DRMKMSLayerData  *data   = layer_data;


     int index  = data->layer_index;

     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );


     if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_BUFFERMODE | CLRCF_SOURCE)) {
          int i;
          for (i=0; i<shared->enabled_crtcs; i++) {
               if (shared->mirror_outputs)
                    index = i;

               if (shared->clone_outputs) {
                    ret = drmModeSetCrtc( drmkms->fd, drmkms->encoder[index]->crtc_id, (u32)(long)left_lock->handle, config->source.x, config->source.y,
                                          shared->cloned_connectors, shared->cloned_count, &shared->mode[index] );
               }
               else {
                    ret = drmModeSetCrtc( drmkms->fd, drmkms->encoder[index]->crtc_id, (u32)(long)left_lock->handle, config->source.x, config->source.y,
                                          &drmkms->connector[index]->connector_id, 1, &shared->mode[index] );
               }

               if (ret) {
                    D_PERROR( "DirectFB/DRMKMS: drmModeSetCrtc() failed! (%d)\n", ret );
                    D_DEBUG_AT( DRMKMS_Mode, " crtc_id: %d connector_id %d, mode %dx%d\n", drmkms->encoder[index]->crtc_id, drmkms->connector[index]->connector_id, shared->mode[index].hdisplay, shared->mode[index].vdisplay );
                    return DFB_FAILURE;
               }

               if (!shared->mirror_outputs)
                    break;
          }

          shared->primary_dimension[data->layer_index]  = surface->config.size;
          shared->primary_rect  = config->source;
          shared->primary_fb    = (u32)(long)left_lock->handle;
     }


     return DFB_OK;
}

static DFBResult
drmkmsFlipRegion( CoreLayer             *layer,
                  void                  *driver_data,
                  void                  *layer_data,
                  void                  *region_data,
                  CoreSurface           *surface,
                  DFBSurfaceFlipFlags    flags,
                  const DFBRegion       *left_update,
                  CoreSurfaceBufferLock *left_lock,
                  const DFBRegion       *right_update,
                  CoreSurfaceBufferLock *right_lock )
{
     int               ret, i;
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;
     DRMKMSLayerData  *data   = layer_data;
     unsigned int      plane_mask;
     unsigned int      buffer_index  = data->index;


     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &drmkms->lock );

     plane_mask = 1 << buffer_index;

     while (drmkms->flip_pending & plane_mask) {
          D_DEBUG_AT( DRMKMS_Layer, "  -> waiting for pending flip (previous)\n" );

          direct_waitqueue_wait( &drmkms->wq_event, &drmkms->lock );
     }

     direct_mutex_unlock( &drmkms->lock );


     dfb_surface_ref( surface );
     drmkms->surface[buffer_index] = surface;
     drmkms->surfacebuffer_index[buffer_index] = left_lock->buffer->index;

     /* Task */
     direct_mutex_lock( &drmkms->task_lock );

     drmkms->pending_tasks[buffer_index] = left_lock->task;

     direct_mutex_unlock( &drmkms->task_lock );


     D_DEBUG_AT( DRMKMS_Layer, "  -> calling drmModePageFlip()\n" );

     ret = drmModePageFlip( drmkms->fd, drmkms->encoder[data->layer_index]->crtc_id, (u32)(long)left_lock->handle, DRM_MODE_PAGE_FLIP_EVENT, driver_data );
     if (ret) {
          D_PERROR( "DirectFB/DRMKMS: drmModePageFlip() failed on layer %d!\n", data->index );
          return DFB_FAILURE;
     }

     if (shared->mirror_outputs) {
          for (i=1; i<shared->enabled_crtcs; i++) {
               ret = drmModePageFlip( drmkms->fd, drmkms->encoder[i]->crtc_id, (u32)(long)left_lock->handle, 0, 0);
               if (ret)
                    D_WARN( "DirectFB/DRMKMS: drmModePageFlip() failed for mirror on crtc id %d!\n", drmkms->encoder[i]->crtc_id );
          }
     }

     shared->primary_fb = (u32)(long)left_lock->handle;

     dfb_surface_flip( surface, false );


     direct_mutex_lock( &drmkms->lock );

     drmkms->flip_pending |= plane_mask;

     direct_waitqueue_broadcast( &drmkms->wq_flip );

     if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) {
          while (drmkms->flip_pending & plane_mask) {
               D_DEBUG_AT( DRMKMS_Layer, "  -> waiting for pending flip (WAITFORSYNC)\n" );

               direct_waitqueue_wait( &drmkms->wq_event, &drmkms->lock );
          }
     }

     direct_mutex_unlock( &drmkms->lock );

     return DFB_OK;
}


static int
drmkmsLayerDataSize( void )
{
     return sizeof(DRMKMSLayerData);
}

static DFBResult
drmkmsPlaneInitLayer( CoreLayer                  *layer,
                      void                       *driver_data,
                      void                       *layer_data,
                      DFBDisplayLayerDescription *description,
                      DFBDisplayLayerConfig      *config,
                      DFBColorAdjustment         *adjustment )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;
     DRMKMSLayerData  *data   = layer_data;
     drmModeObjectPropertiesPtr props;

     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );

     data->index       = shared->layerplane_index_count++;
     data->plane_index = shared->plane_index_count++;
     data->level       = data->index;

     D_DEBUG_AT( DRMKMS_Layer, "  -> getting plane with index %d\n", data->plane_index );

     data->plane = drmModeGetPlane(drmkms->fd, drmkms->plane_resources->planes[data->plane_index]);

     D_DEBUG_AT( DRMKMS_Layer, "     ->  plane_id is %d\n", data->plane->plane_id );

     description->type             = DLTF_GRAPHICS;
     description->caps             = DLCAPS_SURFACE | DLCAPS_SCREEN_POSITION | DLCAPS_ALPHACHANNEL;
     description->surface_caps     = DSCAPS_NONE;
     description->surface_accessor = CSAID_LAYER0;

     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "DRMKMS Plane Layer %d", data->plane_index );


     config->flags      = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->width      = dfb_config->mode.width  ?: shared->mode[0].hdisplay;
     config->height     = dfb_config->mode.height ?: shared->mode[0].vdisplay;

     config->pixelformat = dfb_config->mode.format ?: DSPF_ARGB;
     config->buffermode  = DLBM_FRONTONLY;


     props = drmModeObjectGetProperties( drmkms->fd, data->plane->plane_id, DRM_MODE_OBJECT_PLANE );
     if (props) {
          int                i;
          drmModePropertyPtr prop;

          D_INFO( "DirectFB/DRMKMS: supported properties for layer id %d\n", data->plane->plane_id );
          for (i = 0; i < props->count_props; i++) {
               prop = drmModeGetProperty( drmkms->fd, props->props[i] );
               if (!strcmp(prop->name, "colorkey")) {
                    description->caps |= DLCAPS_SRC_COLORKEY;
                    data->colorkey_propid = prop->prop_id;
                    D_INFO( "     colorkey\n" );
               }
               else if (!strcmp(prop->name, "zpos")) {
                    description->caps |= DLCAPS_LEVELS;
                    data->zpos_propid = prop->prop_id;
                    D_INFO( "     zpos\n" );

                    drmModeObjectSetProperty( drmkms->fd, data->plane->plane_id, DRM_MODE_OBJECT_PLANE, data->zpos_propid, data->level );
               }
               else if (!strcmp(prop->name, "alpha")) {
                    description->caps |= DLCAPS_OPACITY;
                    data->alpha_propid = prop->prop_id;
                    D_INFO( "     alpha\n" );
               }

               drmModeFreeProperty( prop );
          }
          drmModeFreeObjectProperties( props );
     }

     shared->layer_data[data->index] = data;

     return DFB_OK;
}


static DFBResult
drmkmsPlaneGetLevel( CoreLayer *layer,
                     void      *driver_data,
                     void      *layer_data,
                     int       *level )
{
     DRMKMSLayerData  *data   = layer_data;

     if (level)
          *level = data->level;

     return DFB_OK;
}


static DFBResult
drmkmsPlaneSetLevel( CoreLayer *layer,
                     void      *driver_data,
                     void      *layer_data,
                     int        level )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared = drmkms->shared;
     DRMKMSLayerData  *data   = layer_data;
     int               ret;

     if (!data->zpos_propid)
          return DFB_UNSUPPORTED;

     if (level < 1 || level > shared->plane_index_count)
          return DFB_INVARG;

     ret = drmModeObjectSetProperty( drmkms->fd, data->plane->plane_id, DRM_MODE_OBJECT_PLANE, data->zpos_propid, level );

     if (ret) {
          D_ERROR( "DirectFB/DRMKMS: drmModeObjectSetProperty() failed setting zpos\n");
          return DFB_FAILURE;
     }

     data->level = level;

     return DFB_OK;
}

static DFBResult
drmkmsPlaneTestRegion( CoreLayer                  *layer,
                       void                       *driver_data,
                       void                       *layer_data,
                       CoreLayerRegionConfig      *config,
                       CoreLayerRegionConfigFlags *ret_failed )
{
     DRMKMSLayerData *data  = layer_data;

     CoreLayerRegionConfigFlags failed = CLRCF_NONE;

     if (((config->options & DLOP_OPACITY     ) && !data->alpha_propid   ) ||
         ((config->options & DLOP_SRC_COLORKEY) && !data->colorkey_propid))
          failed |= CLRCF_OPTIONS;

     if (ret_failed)
          *ret_failed = failed;

     if (failed)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
drmkmsPlaneSetRegion( CoreLayer                  *layer,
                      void                       *driver_data,
                      void                       *layer_data,
                      void                       *region_data,
                      CoreLayerRegionConfig      *config,
                      CoreLayerRegionConfigFlags  updated,
                      CoreSurface                *surface,
                      CorePalette                *palette,
                      CoreSurfaceBufferLock      *left_lock,
                      CoreSurfaceBufferLock      *right_lock )
{
     int              ret;
     DRMKMSData      *drmkms = driver_data;
     DRMKMSLayerData *data   = layer_data;

     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );
     if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_BUFFERMODE | CLRCF_DEST | CLRCF_SOURCE)) {
          ret = drmModeSetPlane(drmkms->fd, data->plane->plane_id, drmkms->encoder[0]->crtc_id, (u32)(long)left_lock->handle,
                                /* plane_flags */ 0, config->dest.x, config->dest.y, config->dest.w, config->dest.h,
                                config->source.x << 16, config->source.y <<16, config->source.w << 16, config->source.h << 16);

          if (ret) {
               D_INFO( "DirectFB/DRMKMS: drmModeSetPlane(plane_id=%d, fb_id=%d ,  dest=%d,%d-%dx%d, src=%d,%d-%dx%d) failed! (%d)\n", data->plane->plane_id, (u32)(long)left_lock->handle,
                       DFB_RECTANGLE_VALS(&config->dest), DFB_RECTANGLE_VALS(&config->source), ret );

               return DFB_FAILURE;
          }

          data->config = config;

     }

     if ((updated & (CLRCF_SRCKEY | CLRCF_OPTIONS)) && data->alpha_propid) {
          uint32_t drm_colorkey = config->src_key.r << 16 | config->src_key.g << 8 | config->src_key.b;

          if (config->options & DLOP_SRC_COLORKEY)
               drm_colorkey |= 0x01000000;

          ret = drmModeObjectSetProperty( drmkms->fd, data->plane->plane_id, DRM_MODE_OBJECT_PLANE, data->colorkey_propid, drm_colorkey );

          if (ret) {
               D_ERROR( "DirectFB/DRMKMS: drmModeObjectSetProperty() failed setting colorkey\n");
               return DFB_FAILURE;
          }
     }

     if (updated & CLRCF_OPACITY && data->alpha_propid) {
          ret = drmModeObjectSetProperty( drmkms->fd, data->plane->plane_id, DRM_MODE_OBJECT_PLANE, data->alpha_propid, config->opacity );

          if (ret) {
               D_ERROR( "DirectFB/DRMKMS: drmModeObjectSetProperty() failed setting alpha\n");
               return DFB_FAILURE;
          }
     }


     return DFB_OK;
}

static DFBResult
drmkmsPlaneRemoveRegion( CoreLayer             *layer,
                         void                  *driver_data,
                         void                  *layer_data,
                         void                  *region_data )
{
     DFBResult        ret;
     DRMKMSData      *drmkms = driver_data;
     DRMKMSLayerData *data   = layer_data;

     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );


     ret = drmModeSetPlane(drmkms->fd, data->plane->plane_id, drmkms->encoder[0]->crtc_id, 0,
                           /* plane_flags */ 0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0);

     if (ret) {
          D_PERROR( "DRMKMS/Layer/Remove: Failed setting plane configuration!\n" );
          return ret;
     }

     return DFB_OK;
}


static DFBResult
drmkmsPlaneFlipRegion( CoreLayer             *layer,
                       void                  *driver_data,
                       void                  *layer_data,
                       void                  *region_data,
                       CoreSurface           *surface,
                       DFBSurfaceFlipFlags    flags,
                       const DFBRegion       *left_update,
                       CoreSurfaceBufferLock *left_lock,
                       const DFBRegion       *right_update,
                       CoreSurfaceBufferLock *right_lock )
{
     int               ret;
     DRMKMSData       *drmkms = driver_data;
     DRMKMSLayerData  *data   = layer_data;


     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );

     ret = drmModeSetPlane(drmkms->fd, data->plane->plane_id, drmkms->encoder[0]->crtc_id, (u32)(long)left_lock->handle,
                           /* plane_flags */ 0, data->config->dest.x, data->config->dest.y, data->config->dest.w, data->config->dest.h,
                           data->config->source.x << 16, data->config->source.y <<16, data->config->source.w << 16, data->config->source.h << 16);
     if (ret) {
          D_PERROR( "DRMKMS/Layer/FlipRegion: Failed setting plane configuration!\n" );
          return ret;
     }
     dfb_surface_flip( surface, false );

     return DFB_OK;
}


static const DisplayLayerFuncs _drmkmsLayerFuncs = {
     .LayerDataSize = drmkmsLayerDataSize,
     .InitLayer     = drmkmsInitLayer,
     .TestRegion    = drmkmsTestRegion,
     .SetRegion     = drmkmsSetRegion,
     .FlipRegion    = drmkmsFlipRegion
};

static const DisplayLayerFuncs _drmkmsPlaneLayerFuncs = {
     .LayerDataSize = drmkmsLayerDataSize,
     .InitLayer     = drmkmsPlaneInitLayer,
     .GetLevel      = drmkmsPlaneGetLevel,
     .SetLevel      = drmkmsPlaneSetLevel,
     .TestRegion    = drmkmsPlaneTestRegion,
     .SetRegion     = drmkmsPlaneSetRegion,
     .RemoveRegion  = drmkmsPlaneRemoveRegion,
     .FlipRegion    = drmkmsPlaneFlipRegion
};

const DisplayLayerFuncs *drmkmsLayerFuncs = &_drmkmsLayerFuncs;
const DisplayLayerFuncs *drmkmsPlaneLayerFuncs = &_drmkmsPlaneLayerFuncs;

