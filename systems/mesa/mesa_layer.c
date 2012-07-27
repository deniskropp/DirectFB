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


#include "mesa_system.h"

D_DEBUG_DOMAIN( Mesa_Layer, "Mesa/Layer", "Mesa Layer" );

/**********************************************************************************************************************/

void
page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *driver_data);

void
page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *driver_data)
{
     MesaData          *mesa   = driver_data;
     CoreSurfaceBuffer *buffer = mesa->buffer;

     D_DEBUG_AT( Mesa_Layer, "%s()\n", __FUNCTION__ );

     dfb_surface_notify_display( buffer->surface, buffer );

     mesa->flip_pending = false;
     mesa->buffer       = NULL;

     dfb_surface_buffer_unref( buffer );
}


static void *
Mesa_BufferThread_Main( DirectThread *thread, void *arg )
{
     MesaData *data = arg;

     D_DEBUG_AT( Mesa_Layer, "%s()\n", __FUNCTION__ );

     while (true) {
          direct_mutex_lock( &data->lock );

          while (!data->flip_pending) {
               D_DEBUG_AT( Mesa_Layer, "  -> waiting for flip to be issued\n" );

               direct_waitqueue_wait( &data->wq_flip, &data->lock );
          }

          direct_mutex_unlock( &data->lock );


          D_DEBUG_AT( Mesa_Layer, "  -> waiting for flip to be done\n" );

          drmHandleEvent( data->fd, &data->drmeventcontext );


          direct_mutex_lock( &data->lock );

          data->flip_pending = false;

          direct_waitqueue_broadcast( &data->wq_event );

          direct_mutex_unlock( &data->lock );
     }

     return NULL;
}



static DFBResult
mesaInitLayer( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               DFBDisplayLayerDescription *description,
               DFBDisplayLayerConfig      *config,
               DFBColorAdjustment         *adjustment )
{
     MesaData *mesa = driver_data;


     mesa->drmeventcontext.version = DRM_EVENT_CONTEXT_VERSION;
     mesa->drmeventcontext.vblank_handler = NULL;
     mesa->drmeventcontext.page_flip_handler = page_flip_handler;

     description->type             = DLTF_GRAPHICS;
     description->caps             = DLCAPS_SURFACE;
     description->surface_caps     = DSCAPS_NONE;
     description->surface_accessor = CSAID_LAYER0;

     direct_snputs( description->name, "Mesa Layer", DFB_DISPLAY_LAYER_DESC_NAME_LENGTH );


     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT;
     config->width       = dfb_config->mode.width  ?: mesa->mode.hdisplay;
     config->height      = dfb_config->mode.height ?: mesa->mode.vdisplay;
     config->pixelformat = dfb_config->mode.format ?: DSPF_ARGB;
     config->buffermode  = DLBM_FRONTONLY;


     direct_mutex_init( &mesa->lock );
     direct_waitqueue_init( &mesa->wq_event );
     direct_waitqueue_init( &mesa->wq_flip );

     mesa->thread = direct_thread_create( DTT_CRITICAL, Mesa_BufferThread_Main, mesa, "Mesa/Buffer" );

     return DFB_OK;
}

static DFBResult
mesaTestRegion( CoreLayer                  *layer,
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
mesaSetRegion( CoreLayer                  *layer,
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
     MesaData *mesa = driver_data;

     D_DEBUG_AT( Mesa_Layer, "%s()\n", __FUNCTION__ );

     ret = drmModeSetCrtc( mesa->fd, mesa->encoder->crtc_id, (u32)(long)left_lock->handle, 0, 0,
                           &mesa->connector->connector_id, 1, &mesa->mode );
     if (ret) {
          D_PERROR( "DirectFB/Mesa: drmModeSetCrtc() failed! (%d)\n", ret );
          return DFB_FAILURE;
     }

     return DFB_OK;
}

static DFBResult
mesaFlipRegion( CoreLayer             *layer,
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
     MesaData      *mesa = driver_data;

     D_DEBUG_AT( Mesa_Layer, "%s()\n", __FUNCTION__ );

     //    ret = drmModeSetCrtc( mesa->fd, mesa->encoder->crtc_id, (u32)(long)left_lock->handle, 0, 0,
     //                           &mesa->connector->connector_id, 1, &mesa->mode );

     direct_mutex_lock( &mesa->lock );

     while (mesa->flip_pending) {
          D_DEBUG_AT( Mesa_Layer, "  -> waiting for pending flip (previous)\n" );

          direct_waitqueue_wait( &mesa->wq_event, &mesa->lock );
     }

     direct_mutex_unlock( &mesa->lock );


     D_ASSERT( mesa->buffer == NULL );


     mesa->buffer = left_lock->buffer;
     dfb_surface_buffer_ref( mesa->buffer );


     D_DEBUG_AT( Mesa_Layer, "  -> calling drmModePageFlip()\n" );

     ret = drmModePageFlip( mesa->fd, mesa->encoder->crtc_id, (u32)(long)left_lock->handle, DRM_MODE_PAGE_FLIP_EVENT, driver_data );
     if (ret) {
          D_PERROR( "DirectFB/Mesa: drmModePageFlip() failed!\n" );
          return DFB_FAILURE;
     }

     dfb_surface_flip( surface, false );


     direct_mutex_lock( &mesa->lock );

     mesa->flip_pending = true;

     direct_waitqueue_broadcast( &mesa->wq_flip );

     if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) {
          while (mesa->flip_pending) {
               D_DEBUG_AT( Mesa_Layer, "  -> waiting for pending flip (WAITFORSYNC)\n" );

               direct_waitqueue_wait( &mesa->wq_event, &mesa->lock );
          }
     }

     direct_mutex_unlock( &mesa->lock );

     return DFB_OK;
}

static const DisplayLayerFuncs _mesaLayerFuncs = {
     .InitLayer     = mesaInitLayer,
     .TestRegion    = mesaTestRegion,
     .SetRegion     = mesaSetRegion,
     .FlipRegion    = mesaFlipRegion
};

const DisplayLayerFuncs *mesaLayerFuncs = &_mesaLayerFuncs;

