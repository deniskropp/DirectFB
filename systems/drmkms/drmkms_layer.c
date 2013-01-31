/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <config.h>

#include <core/layers.h>
#include <core/screens.h>

#include <misc/conf.h>


#include "drmkms_system.h"

D_DEBUG_DOMAIN( DRMKMS_Layer, "DRMKMS/Layer", "DRM/KMS Layer" );
D_DEBUG_DOMAIN( DRMKMS_Mode, "DRMKMS/Mode", "DRM/KMS Mode" );

/**********************************************************************************************************************/

void
page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *driver_data);

void
page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *driver_data)
{
     DRMKMSData          *drmkms   = driver_data;
     CoreSurfaceBuffer *buffer = drmkms->buffer;

     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );

     dfb_surface_notify_display( buffer->surface, buffer );

     drmkms->flip_pending = false;
     drmkms->buffer       = NULL;

     dfb_surface_buffer_unref( buffer );
}


static void *
DRMKMS_BufferThread_Main( DirectThread *thread, void *arg )
{
     DRMKMSData *data = arg;

     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );

     while (true) {
          direct_mutex_lock( &data->lock );

          while (!data->flip_pending) {
               D_DEBUG_AT( DRMKMS_Layer, "  -> waiting for flip to be issued\n" );

               direct_waitqueue_wait( &data->wq_flip, &data->lock );
          }

          direct_mutex_unlock( &data->lock );


          D_DEBUG_AT( DRMKMS_Layer, "  -> waiting for flip to be done\n" );

          drmHandleEvent( data->fd, &data->drmeventcontext );


          direct_mutex_lock( &data->lock );

          data->flip_pending = false;

          direct_waitqueue_broadcast( &data->wq_event );

          direct_mutex_unlock( &data->lock );
     }

     return NULL;
}



static DFBResult
drmkmsInitLayer( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               DFBDisplayLayerDescription *description,
               DFBDisplayLayerConfig      *config,
               DFBColorAdjustment         *adjustment )
{
     DRMKMSData *drmkms = driver_data;


     drmkms->drmeventcontext.version = DRM_EVENT_CONTEXT_VERSION;
     drmkms->drmeventcontext.vblank_handler = NULL;
     drmkms->drmeventcontext.page_flip_handler = page_flip_handler;

     description->type             = DLTF_GRAPHICS;
     description->caps             = DLCAPS_SURFACE;
     description->surface_caps     = DSCAPS_NONE;
     description->surface_accessor = CSAID_LAYER0;

     direct_snputs( description->name, "DRMKMS Layer", DFB_DISPLAY_LAYER_DESC_NAME_LENGTH );


     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT;
     config->width      = dfb_config->mode.width  ?: drmkms->mode.hdisplay;
     config->height     = dfb_config->mode.height ?: drmkms->mode.vdisplay;

     config->pixelformat = dfb_config->mode.format ?: DSPF_ARGB;
     config->buffermode  = DLBM_FRONTONLY;


     direct_mutex_init( &drmkms->lock );
     direct_waitqueue_init( &drmkms->wq_event );
     direct_waitqueue_init( &drmkms->wq_flip );

     drmkms->thread = direct_thread_create( DTT_CRITICAL, DRMKMS_BufferThread_Main, drmkms, "DRMKMS/Buffer" );

     return DFB_OK;
}

static DFBResult
drmkmsTestRegion( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  CoreLayerRegionConfig      *config,
                  CoreLayerRegionConfigFlags *ret_failed )
{
     if (ret_failed)
          *ret_failed = DLCONF_NONE;

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
     int       ret;
     DRMKMSData *drmkms = driver_data;

     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );


     if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT))
     {
          drmkms->mode = *drmkms_find_mode (config->source.w, config->source.h);
          ret = drmModeSetCrtc( drmkms->fd, drmkms->encoder->crtc_id, (u32)(long)left_lock->handle, 0, 0,
                                &drmkms->connector->connector_id, 1, &drmkms->mode );
          if (ret) {
               D_PERROR( "DirectFB/DRMKMS: drmModeSetCrtc() failed! (%d)\n", ret );
               D_DEBUG_AT( DRMKMS_Mode, " crtc_id: %d connector_id %d, mode %dx%d\n", drmkms->encoder->crtc_id, drmkms->connector->connector_id, drmkms->mode.hdisplay, drmkms->mode.vdisplay );
               return DFB_FAILURE;
          }

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
     int            ret;
     DRMKMSData      *drmkms = driver_data;

     D_DEBUG_AT( DRMKMS_Layer, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &drmkms->lock );

     while (drmkms->flip_pending) {
          D_DEBUG_AT( DRMKMS_Layer, "  -> waiting for pending flip (previous)\n" );

          direct_waitqueue_wait( &drmkms->wq_event, &drmkms->lock );
     }

     direct_mutex_unlock( &drmkms->lock );


     D_ASSERT( drmkms->buffer == NULL );


     drmkms->buffer = left_lock->buffer;
     dfb_surface_buffer_ref( drmkms->buffer );


     D_DEBUG_AT( DRMKMS_Layer, "  -> calling drmModePageFlip()\n" );

     ret = drmModePageFlip( drmkms->fd, drmkms->encoder->crtc_id, (u32)(long)left_lock->handle, DRM_MODE_PAGE_FLIP_EVENT, driver_data );
     if (ret) {
          D_PERROR( "DirectFB/DRMKMS: drmModePageFlip() failed!\n" );
          return DFB_FAILURE;
     }

     dfb_surface_flip( surface, false );


     direct_mutex_lock( &drmkms->lock );

     drmkms->flip_pending = true;

     direct_waitqueue_broadcast( &drmkms->wq_flip );

     if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) {
          while (drmkms->flip_pending) {
               D_DEBUG_AT( DRMKMS_Layer, "  -> waiting for pending flip (WAITFORSYNC)\n" );

               direct_waitqueue_wait( &drmkms->wq_event, &drmkms->lock );
          }
     }

     direct_mutex_unlock( &drmkms->lock );

     return DFB_OK;
}

static const DisplayLayerFuncs _drmkmsLayerFuncs = {
     .InitLayer     = drmkmsInitLayer,
     .TestRegion    = drmkmsTestRegion,
     .SetRegion     = drmkmsSetRegion,
     .FlipRegion    = drmkmsFlipRegion
};

const DisplayLayerFuncs *drmkmsLayerFuncs = &_drmkmsLayerFuncs;

