/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
	      Michel Dänzer <michel@daenzer.net>.

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

#ifndef ___RADEON_H__
#define ___RADEON_H__

#include <dfb_types.h>
#include <core/coretypes.h>
#include <core/layers.h>

typedef struct {
    volatile __u8 *mmio_base;
} RADEONDriverData;

typedef struct {
    CoreSurface *source;
    CoreSurface *destination;
    DFBSurfaceBlittingFlags blittingflags;

    /* store some Radeon register values in native format */
    __u32 RADEON_dp_gui_master_cntl;
    __u32 RADEON_color_compare;

    /* state validation */
    int v_destination;
    int v_color;
    int v_source;
    int v_src_colorkey;
    int v_blittingflags;

    /* for fifo/performance monitoring */
    unsigned int fifo_space;

    unsigned int waitfifo_sum;
    unsigned int waitfifo_calls;
    unsigned int fifo_waitcycles;
    unsigned int idle_waitcycles;
    unsigned int fifo_cache_hits;
} RADEONDeviceData;

#endif
