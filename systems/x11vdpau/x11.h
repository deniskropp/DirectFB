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

#ifndef __X11SYSTEM__X11_H__
#define __X11SYSTEM__X11_H__

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <vdpau/vdpau_x11.h>

#include <fusion/call.h>
#include <fusion/lock.h>

#include <core/layers.h>
#include <core/surface.h>
#include <core/surface_pool.h>

#include "x11types.h"


typedef struct {
     VdpDevice                                     device;

     uint32_t                                      api_version;
     const char                                   *information_string;


     VdpGetProcAddress                            *GetProcAddress;
     VdpGetErrorString                            *GetErrorString;
     VdpGetApiVersion                             *GetApiVersion;
     VdpGetInformationString                      *GetInformationString;


     VdpPresentationQueueTargetCreateX11          *PresentationQueueTargetCreateX11;
     VdpPresentationQueueTargetDestroy            *PresentationQueueTargetDestroy;


     VdpOutputSurfaceCreate                       *OutputSurfaceCreate;
     VdpOutputSurfaceDestroy                      *OutputSurfaceDestroy;

     VdpOutputSurfaceGetBitsNative                *OutputSurfaceGetBitsNative;
     VdpOutputSurfacePutBitsNative                *OutputSurfacePutBitsNative;

     VdpOutputSurfaceRenderOutputSurface          *OutputSurfaceRenderOutputSurface;


     VdpPresentationQueueCreate                   *PresentationQueueCreate;
     VdpPresentationQueueDestroy                  *PresentationQueueDestroy;

     VdpPresentationQueueDisplay                  *PresentationQueueDisplay;
     VdpPresentationQueueGetTime                  *PresentationQueueGetTime;

     VdpPresentationQueueBlockUntilSurfaceIdle    *PresentationQueueBlockUntilSurfaceIdle;
} DFBX11VDPAU;

typedef struct {
     FusionSkirmish                lock;
     FusionCall                    call;

     CoreSurfacePool              *vdpau_pool;

     DFBDimension                  screen_size;

     Window                        window;
     int                           depth;

     VdpPresentationQueueTarget    vdp_target;
     VdpPresentationQueue          vdp_queue;
     VdpOutputSurface              vdp_surface;
     CoreSurface                  *vdp_core_surface;
} DFBX11Shared;

struct __DFB_X11 {
     DFBX11Shared                 *shared;

     CoreDFB                      *core;
     CoreScreen                   *screen;

     Display                      *display;
     Screen                       *screenptr;
     int                           screennum;

     Visual                       *visuals[DFB_NUM_PIXELFORMATS];

     DFBX11VDPAU                   vdp;
};

typedef enum {
     X11_VDPAU_OUTPUT_SURFACE_CREATE,
     X11_VDPAU_OUTPUT_SURFACE_DESTROY,
     X11_VDPAU_OUTPUT_SURFACE_GET_BITS_NATIVE,
     X11_VDPAU_OUTPUT_SURFACE_PUT_BITS_NATIVE,
     X11_VDPAU_OUTPUT_SURFACE_RENDER_OUTPUT_SURFACE,
     X11_VDPAU_PRESENTATION_QUEUE_DISPLAY,
} DFBX11Call;

typedef struct {
     VdpRGBAFormat      rgba_format;
     uint32_t           width;
     uint32_t           height;
} DFBX11CallOutputSurfaceCreate;

typedef struct {
     VdpOutputSurface   surface;
} DFBX11CallOutputSurfaceDestroy;

typedef struct {
     VdpOutputSurface   surface;
     VdpRect            source_rect;
     void              *ptr;
     unsigned int       pitch;
} DFBX11CallOutputSurfaceGetBitsNative;

typedef struct {
     VdpOutputSurface   surface;
     void              *ptr;
     unsigned int       pitch;
     VdpRect            destination_rect;
} DFBX11CallOutputSurfacePutBitsNative;

typedef struct {
     VdpOutputSurface                 destination_surface;
     VdpRect                          destination_rect;
     VdpOutputSurface                 source_surface;
     VdpRect                          source_rect;
     VdpColor                         color;
     VdpOutputSurfaceRenderBlendState blend_state;
     uint32_t                         flags;
} DFBX11CallOutputSurfaceRenderOutputSurface;

typedef struct {
     VdpPresentationQueue presentation_queue;
     VdpOutputSurface     surface;
     uint32_t             clip_width;
     uint32_t             clip_height;
     VdpTime              earliest_presentation_time;
} DFBX11CallPresentationQueueDisplay;


#endif //__X11SYSTEM__X11_H__

