/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

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

#ifndef __GENERIC_H__
#define __GENERIC_H__

#include <dfb_types.h>

#include <directfb.h>
#include <core/coretypes.h>
#include <core/gfxcard.h>

/* this order is required for Intel with MMX, how about bigendian? */

typedef union {
     struct {
          __u16 b;
          __u16 g;
          __u16 r;
          __u16 a;
     } RGB;
     struct {
          __u16 u;
          __u16 v;
          __u16 y;
          __u16 a;
     } YUV;
} GenefxAccumulator;


typedef struct _GenefxState GenefxState;

typedef void (*GenefxFunc)(GenefxState *gfxs);

/*
 * State of the virtual graphics processing unit "Genefx" (pron. 'genie facts').
 */
struct _GenefxState {
     GenefxFunc funcs[32];

     int length;    /* span length */
     int Slen;      /* span length (source) */
     int Dlen;      /* span length (destination) */

     /*
      * state values
      */
     void *dst_org[3];
     void *src_org[3];
     int dst_pitch;
     int src_pitch;

     int dst_bpp;
     int src_bpp;

     DFBSurfaceCapabilities dst_caps;
     DFBSurfaceCapabilities src_caps;

     DFBSurfacePixelFormat src_format;
     DFBSurfacePixelFormat dst_format;

     int dst_height;
     int src_height;

     int dst_field_offset;
     int src_field_offset;

     DFBColor color;

     /*
      * operands
      */
     void *Aop[3];
     void *Bop[3];
     __u32 Cop;

     __u8 YCop;
     __u8 CbCop;
     __u8 CrCop;

     int Aop_field;
     int Bop_field;
     
     int AopY;
     int BopY;

     /*
      * color keys
      */
     __u32 Dkey;
     __u32 Skey;

     /*
      * color lookup tables
      */
     CorePalette *Alut;
     CorePalette *Blut;

     /*
      * accumulators
      */
     void              *ABstart;
     int                ABsize;
     GenefxAccumulator *Aacc;
     GenefxAccumulator *Bacc;
     GenefxAccumulator  Cacc;
     GenefxAccumulator  SCacc;

     /*
      * dataflow control
      */
     GenefxAccumulator *Xacc;
     GenefxAccumulator *Dacc;
     GenefxAccumulator *Sacc;

     void        **Sop;
     CorePalette  *Slut;

     int Ostep; /* controls horizontal blitting direction */

     int SperD; /* for scaled routines only */
};


void gGetDriverInfo( GraphicsDriverInfo *info );
void gGetDeviceInfo( GraphicsDeviceInfo *info );

bool gAcquire  ( CardState *state, DFBAccelerationMask accel );
void gRelease ( CardState *state );

void gFillRectangle ( CardState *state, DFBRectangle *rect );
void gDrawLine      ( CardState *state, DFBRegion    *line );

void gBlit          ( CardState *state, DFBRectangle *rect, int dx, int dy );
void gStretchBlit   ( CardState *state, DFBRectangle *srect, DFBRectangle *drect );


#endif
