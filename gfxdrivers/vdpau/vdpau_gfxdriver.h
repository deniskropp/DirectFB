/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

#ifndef __VDPAU_GFXDRIVER_H__
#define __VDPAU_GFXDRIVER_H__

#include <x11vdpau/x11.h>

#include <vdpau/vdpau.h>


typedef struct {
     bool                sync;

     VdpOutputSurface    white;

     u32                 pixel;    /* dummy for syncing */
} VDPAUDeviceData;


typedef struct {
     DFBX11             *x11;
     DFBX11VDPAU        *vdp;
     Display            *display;

     /* validation flags */
     int                                        v_flags;

     DFBX11CallOutputSurfaceRenderOutputSurface render_draw;
     DFBX11CallOutputSurfaceRenderOutputSurface render_blit;
} VDPAUDriverData;


#endif
