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

#include "vt.h"

extern const SurfacePoolFuncs drmkmsSurfacePoolFuncs;

extern const ScreenFuncs       *drmkmsScreenFuncs;
extern const DisplayLayerFuncs *drmkmsLayerFuncs;
extern const DisplayLayerFuncs *drmkmsPlaneLayerFuncs;



typedef struct {
     int                  index;
	drmModePlane        *plane;

} DRMKMSPlaneData;

typedef struct {
     FusionSHMPoolShared *shmpool;

     CoreSurfacePool     *pool;

     bool                 use_prime_fd;

     DRMKMSPlaneData      plane_data[16];

} DRMKMSDataShared;

typedef struct {
     DRMKMSDataShared      *shared;

     CoreDFB             *core;
     CoreScreen          *screen;
     CoreLayer           *layer;

     int                  fd;      /* DRM file descriptor */

#ifdef USE_GBM
     struct gbm_device   *gbm;
#else
     struct kms_driver   *kms;
#endif

     drmModeConnector    *connector;
     drmModeEncoder      *encoder;
     drmModeModeInfo     mode;
     drmModeRes          *resources;
     drmModePlaneRes     *plane_resources;

     drmModeCrtcPtr       saved_crtc;

     drmEventContext      drmeventcontext;

     VirtualTerminal     *vt;

     unsigned int         flip_pending;
     unsigned int         plane_flip_pending_mask;

     CoreSurfaceBuffer   *buffer[16];

     DirectThread        *thread;
     DirectMutex          lock;
     DirectWaitQueue      wq_event;
     DirectWaitQueue      wq_flip;

     int                  plane_index_count;
} DRMKMSData;


const drmModeModeInfo*
drmkms_find_mode( int width, int height );

#endif
