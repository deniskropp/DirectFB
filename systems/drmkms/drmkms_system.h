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

#ifndef __DRMKMS_SYSTEM_H__
#define __DRMKMS_SYSTEM_H__

#ifdef USE_GBM
#include <gbm.h>
#else
#include <libkms/libkms.h>
#endif


#include <xf86drmMode.h>
#include <xf86drm.h>

#include <fusion/shmalloc.h>

#include <core/surface_pool.h>

#include <core/layers.h>
#include <core/screens.h>
#include <core/Task.h>

#include "vt.h"

extern const SurfacePoolFuncs   drmkmsSurfacePoolFuncs;

extern const ScreenFuncs       *drmkmsScreenFuncs;
extern const DisplayLayerFuncs *drmkmsLayerFuncs;
extern const DisplayLayerFuncs *drmkmsPlaneLayerFuncs;



typedef struct {
     int                  index;
     int                  layer_index;
     int                  plane_index;

     drmModePlane        *plane;
     uint32_t             colorkey_propid;
     uint32_t             zpos_propid;

     int                  level;
} DRMKMSLayerData;

typedef struct {
     FusionSHMPoolShared *shmpool;

     CoreSurfacePool     *pool;

     bool                 use_prime_fd;

     bool                 mirror_outputs;
     bool                 clone_outputs;
     bool                 multihead;
     int                  plane_limit;

     char                 device_name[256];

     DFBRectangle         primary_rect;

     u32                  primary_fb;

     drmModeModeInfo      mode[8];
     DFBDimension         primary_dimension[8];

     int                  enabled_encoders;

     uint32_t             cloned_connectors[8];
     int                  cloned_count;

     DRMKMSLayerData     *layer_data[16];

     int                  layer_index_count;
     int                  plane_index_count;
     int                  layerplane_index_count;
} DRMKMSDataShared;

typedef struct {
     DRMKMSDataShared    *shared;

     CoreDFB             *core;
     CoreScreen          *screen;

     int                  fd;      /* DRM file descriptor */

#ifdef USE_GBM
     struct gbm_device   *gbm;
#else
     struct kms_driver   *kms;
#endif

     drmModeConnector    *connector[8];
     drmModeEncoder      *encoder[8];

     drmModeRes          *resources;
     drmModePlaneRes     *plane_resources;

     drmModeCrtcPtr       saved_crtc;

     drmEventContext      drmeventcontext;

     VirtualTerminal     *vt;

     unsigned int         flip_pending;
     unsigned int         plane_flip_pending_mask;
     CoreSurface         *surface[16];
     int                  surfacebuffer_index[16];

     DFB_DisplayTask     *prev_tasks[16];
     DFB_DisplayTask     *pending_tasks[16];
     DirectMutex          task_lock;

     DirectThread        *thread;
     DirectMutex          lock;
     DirectWaitQueue      wq_event;
     DirectWaitQueue      wq_flip;
} DRMKMSData;


drmModeModeInfo*
drmkms_find_mode( int encoder, int width, int height, int freq );

drmModeModeInfo*
drmkms_dsor_freq_to_mode( int encoder, DFBScreenOutputResolution dsor, DFBScreenEncoderFrequency freq );

DFBResult
drmkms_mode_to_dsor_dsef( drmModeModeInfo *videomode, DFBScreenOutputResolution *dso_res,  DFBScreenEncoderFrequency *dse_freq );

DFBScreenOutputResolution
drmkms_modes_to_dsor_bitmask( int connector );

#endif
