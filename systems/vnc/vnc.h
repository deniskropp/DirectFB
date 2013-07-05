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



#ifndef __VNC__VNC_H__
#define __VNC__VNC_H__

#include <directfb.h>

#include <rfb/rfb.h>

#include <fusion/call.h>
#include <fusion/lock.h>

#include <core/layers.h>
#include <core/screens.h>

#define VNC_MAX_LAYERS 2

typedef struct {
     bool                     shown;
     CoreLayerRegionConfig    config;

     DFBDisplayLayerID        layer_id;

     CoreSurface             *surface;
} VNCLayerData;


typedef struct {
     FusionCall          call;

     DFBDimension        screen_size;
     CoreSurface        *screen_surface;

     VNCLayerData       *layer_data[VNC_MAX_LAYERS];
} DFBVNCShared;

typedef struct {
     DFBVNCShared       *shared;

     CoreDFB            *core;

     CoreScreen         *screen;
     CoreLayer          *layer[VNC_MAX_LAYERS];

     rfbScreenInfoPtr    rfb_screen;

     unsigned int        layer_count;

     CoreSurfaceBufferLock  buffer_lock;
} DFBVNC;

typedef enum {
     VNC_MARK_RECT_AS_MODIFIED,
} DFBVNCCall;

typedef struct {
     DFBRegion           region;
} DFBVNCMarkRectAsModified;

#endif

