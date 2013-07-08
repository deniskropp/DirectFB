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



#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dfb_types.h>

#include <pthread.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/state.h>
#include <core/palette.h>

#include <misc/gfx_util.h>
#include <misc/util.h>
#include <misc/conf.h>

#include <direct/clock.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include "generic.h"

/**********************************************************************************************************************/
/**********************************************************************************************************************/

void Genefx_Aop_xy( GenefxState *gfxs, int x, int y )
{
     int pitch = gfxs->dst_pitch;

     gfxs->Aop[0] = gfxs->dst_org[0];
     gfxs->AopY   = y;

     if (gfxs->dst_caps & DSCAPS_SEPARATED) {
          gfxs->Aop_field = y & 1;
          if (gfxs->Aop_field)
               gfxs->Aop[0] += gfxs->dst_field_offset;

          y /= 2;
     }

     D_ASSUME( !(x & DFB_PIXELFORMAT_ALIGNMENT(gfxs->dst_format)) );

     gfxs->Aop[0] += y * pitch + DFB_BYTES_PER_LINE( gfxs->dst_format, x );

     if (DFB_PLANAR_PIXELFORMAT(gfxs->dst_format)) {
          int dst_field_offset = gfxs->dst_field_offset;

          switch (gfxs->dst_format) {
               case DSPF_YV12:
               case DSPF_I420:
                    dst_field_offset /= 4;
                    pitch /= 2;
                    y /= 2;
                    x /= 2;
                    break;
               case DSPF_YV16:
                    dst_field_offset /= 2;
                    pitch /= 2;
                    x /= 2;
                    break;
               case DSPF_NV12:
               case DSPF_NV21:
                    dst_field_offset /= 2;
                    y /= 2;
               case DSPF_NV16:
                    x &= ~1;
                    break;
               case DSPF_YUV444P: /* nothing to adjust */
               default:
                    break;
          }

          gfxs->Aop[1] = gfxs->dst_org[1];
          gfxs->Aop[2] = gfxs->dst_org[2];

          if (gfxs->dst_caps & DSCAPS_SEPARATED && gfxs->Aop_field) {
               gfxs->Aop[1] += dst_field_offset;
               gfxs->Aop[2] += dst_field_offset;
          }

          gfxs->Aop[1] += y * pitch + x;
          gfxs->Aop[2] += y * pitch + x;
     }
}

void Genefx_Aop_crab( GenefxState *gfxs )
{
     gfxs->Aop[0] += gfxs->dst_bpp;
     gfxs->AopY++;
}

void Genefx_Aop_prev_crab( GenefxState *gfxs )
{
     gfxs->Aop[0] -= gfxs->dst_bpp;
     gfxs->AopY++;
}

void Genefx_Aop_next( GenefxState *gfxs )
{
     int pitch = gfxs->dst_pitch;

     if (gfxs->dst_caps & DSCAPS_SEPARATED) {
          gfxs->Aop_field++;

          if (gfxs->Aop_field & 1)
               gfxs->Aop[0] += gfxs->dst_field_offset;
          else
               gfxs->Aop[0] += pitch - gfxs->dst_field_offset;
     }
     else
          gfxs->Aop[0] += pitch;

     if (DFB_PLANAR_PIXELFORMAT(gfxs->dst_format)) {
          if (gfxs->dst_format == DSPF_YV12 || gfxs->dst_format == DSPF_I420) {
               if (gfxs->AopY & 1) {
                    if (gfxs->dst_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Aop_field & 2) {
                              gfxs->Aop[1] += gfxs->dst_field_offset/4;
                              gfxs->Aop[2] += gfxs->dst_field_offset/4;
                         }
                         else {
                              gfxs->Aop[1] += pitch/2 - gfxs->dst_field_offset/4;
                              gfxs->Aop[2] += pitch/2 - gfxs->dst_field_offset/4;
                         }
                    }
                    else {
                         gfxs->Aop[1] += pitch/2;
                         gfxs->Aop[2] += pitch/2;
                    }
               }
          }
          else if (gfxs->dst_format == DSPF_YV16) {
               if (gfxs->dst_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Aop_field & 2) {
                         gfxs->Aop[1] += gfxs->dst_field_offset/2;
                         gfxs->Aop[2] += gfxs->dst_field_offset/2;
                    }
                    else {
                         gfxs->Aop[1] += pitch/2 - gfxs->dst_field_offset/2;
                         gfxs->Aop[2] += pitch/2 - gfxs->dst_field_offset/2;
                    }
               }
               else {
                    gfxs->Aop[1] += pitch/2;
                    gfxs->Aop[2] += pitch/2;
               }
          }
          else if (gfxs->dst_format == DSPF_NV12 || gfxs->dst_format == DSPF_NV21) {
               if (gfxs->AopY & 1) {
                    if (gfxs->dst_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Aop_field & 2)
                              gfxs->Aop[1] += gfxs->dst_field_offset/2;
                         else
                              gfxs->Aop[1] += pitch - gfxs->dst_field_offset/2;
                    }
                    else {
                         gfxs->Aop[1] += pitch;
                    }
               }
          }
          else if (gfxs->dst_format == DSPF_YUV444P) {
               if (gfxs->dst_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Aop_field & 1) {
                         gfxs->Aop[1] += gfxs->dst_field_offset;
                         gfxs->Aop[2] += gfxs->dst_field_offset;
                    }
                    else {
                         gfxs->Aop[1] += pitch - gfxs->dst_field_offset;
                         gfxs->Aop[2] += pitch - gfxs->dst_field_offset;
                    }
               }
               else {
                    gfxs->Aop[1] += pitch;
                    gfxs->Aop[2] += pitch;
               }
          }
          else { /* NV16 */
               if (gfxs->dst_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Aop_field & 1)
                         gfxs->Aop[1] += gfxs->dst_field_offset;
                    else
                         gfxs->Aop[1] += pitch - gfxs->dst_field_offset;
               }
               else {
                    gfxs->Aop[1] += pitch;
               }
          }
     }

     gfxs->AopY++;
}

void Genefx_Aop_prev( GenefxState *gfxs )
{
     int pitch = gfxs->dst_pitch;

     if (gfxs->dst_caps & DSCAPS_SEPARATED) {
          gfxs->Aop_field++;

          if (gfxs->Aop_field & 1)
               gfxs->Aop[0] += gfxs->dst_field_offset - pitch;
          else
               gfxs->Aop[0] -= gfxs->dst_field_offset;
     }
     else
          gfxs->Aop[0] -= pitch;

     if (DFB_PLANAR_PIXELFORMAT(gfxs->dst_format)) {
          if (gfxs->dst_format == DSPF_YV12 || gfxs->dst_format == DSPF_I420) {
               if (gfxs->AopY & 1) {
                    if (gfxs->dst_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Aop_field & 2) {
                              gfxs->Aop[1] += gfxs->dst_field_offset/4 - pitch/2;
                              gfxs->Aop[2] += gfxs->dst_field_offset/4 - pitch/2;
                         }
                         else {
                              gfxs->Aop[1] -= gfxs->dst_field_offset/4;
                              gfxs->Aop[2] -= gfxs->dst_field_offset/4;
                         }
                    }
                    else {
                         gfxs->Aop[1] -= pitch/2;
                         gfxs->Aop[2] -= pitch/2;
                    }
               }
          }
          else if (gfxs->dst_format == DSPF_YV16) {
               if (gfxs->dst_caps & DSCAPS_SEPARATED) {
                   if (gfxs->Aop_field & 2) {
                        gfxs->Aop[1] += gfxs->dst_field_offset/2 - pitch/2;
                        gfxs->Aop[2] += gfxs->dst_field_offset/2 - pitch/2;
                   }
                   else {
                        gfxs->Aop[1] -= gfxs->dst_field_offset/2;
                        gfxs->Aop[2] -= gfxs->dst_field_offset/2;
                   }
               }
               else {
                    gfxs->Aop[1] -= pitch/2;
                    gfxs->Aop[2] -= pitch/2;
               }
          }
          else if (gfxs->dst_format == DSPF_NV12 || gfxs->dst_format == DSPF_NV21) {
               if (gfxs->AopY & 1) {
                    if (gfxs->dst_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Aop_field & 2)
                              gfxs->Aop[1] += gfxs->dst_field_offset/2 - pitch;
                         else
                              gfxs->Aop[1] -= gfxs->dst_field_offset/2;
                    }
                    else {
                         gfxs->Aop[1] -= pitch;
                    }
               }
          }
          else if (gfxs->dst_format == DSPF_YUV444P) {
               if (gfxs->dst_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Aop_field & 1) {
                         gfxs->Aop[1] += gfxs->dst_field_offset - pitch;
                         gfxs->Aop[2] += gfxs->dst_field_offset - pitch;
                    }
                    else {
                         gfxs->Aop[1] -= gfxs->dst_field_offset;
                         gfxs->Aop[2] -= gfxs->dst_field_offset;
                    }
               }
               else {
                    gfxs->Aop[1] -= pitch;
                    gfxs->Aop[2] -= pitch;
               }
          }
          else { /* NV16 */
               if (gfxs->dst_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Aop_field & 1)
                         gfxs->Aop[1] += gfxs->dst_field_offset - pitch;
                    else
                         gfxs->Aop[1] -= gfxs->dst_field_offset;
               }
               else {
                    gfxs->Aop[1] -= pitch;
               }
          }
     }

     gfxs->AopY--;
}


void Genefx_Bop_xy( GenefxState *gfxs, int x, int y )
{
     int pitch = gfxs->src_pitch;

     gfxs->Bop[0] = gfxs->src_org[0];
     gfxs->BopY   = y;

     if (gfxs->src_caps & DSCAPS_SEPARATED) {
          gfxs->Bop_field = y & 1;
          if (gfxs->Bop_field)
               gfxs->Bop[0] += gfxs->src_field_offset;

          y /= 2;
     }

     D_ASSUME( !(x & DFB_PIXELFORMAT_ALIGNMENT(gfxs->src_format)) );

     gfxs->Bop[0] += y * pitch + DFB_BYTES_PER_LINE( gfxs->src_format, x );

     if (DFB_PLANAR_PIXELFORMAT(gfxs->src_format)) {
          int src_field_offset = gfxs->src_field_offset;

          switch (gfxs->src_format) {
               case DSPF_YV12:
               case DSPF_I420:
                    src_field_offset /= 4;
                    pitch /= 2;
                    y /= 2;
                    x /= 2;
                    break;
               case DSPF_YV16:
                    src_field_offset /= 2;
                    pitch /= 2;
                    x /= 2;
                    break;
               case DSPF_NV12:
               case DSPF_NV21:
                    src_field_offset /= 2;
                    y /= 2;
               case DSPF_NV16:
                    x &= ~1;
                    break;
               case DSPF_YUV444P: /* nothing to adjust */
               default:
                    break;
          }

          gfxs->Bop[1] = gfxs->src_org[1];
          gfxs->Bop[2] = gfxs->src_org[2];

          if (gfxs->src_caps & DSCAPS_SEPARATED && gfxs->Bop_field) {
               gfxs->Bop[1] += src_field_offset;
               gfxs->Bop[2] += src_field_offset;
          }

          gfxs->Bop[1] += y * pitch + x;
          gfxs->Bop[2] += y * pitch + x;
     }
}

void Genefx_Bop_next( GenefxState *gfxs )
{
     int pitch = gfxs->src_pitch;

     if (gfxs->src_caps & DSCAPS_SEPARATED) {
          gfxs->Bop_field++;

          if (gfxs->Bop_field & 1)
               gfxs->Bop[0] += gfxs->src_field_offset;
          else
               gfxs->Bop[0] += pitch - gfxs->src_field_offset;
     }
     else
          gfxs->Bop[0] += pitch;

     if (DFB_PLANAR_PIXELFORMAT(gfxs->src_format)) {
          if (gfxs->src_format == DSPF_YV12 || gfxs->src_format == DSPF_I420) {
               if (gfxs->BopY & 1) {
                    if (gfxs->src_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Bop_field & 2) {
                              gfxs->Bop[1] += gfxs->src_field_offset/4;
                              gfxs->Bop[2] += gfxs->src_field_offset/4;
                         }
                         else {
                              gfxs->Bop[1] += pitch/2 - gfxs->src_field_offset/4;
                              gfxs->Bop[2] += pitch/2 - gfxs->src_field_offset/4;
                         }
                    }
                    else {
                         gfxs->Bop[1] += pitch/2;
                         gfxs->Bop[2] += pitch/2;
                    }
               }
          }
          else if (gfxs->src_format == DSPF_YV16) {
               if (gfxs->src_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Bop_field & 2) {
                         gfxs->Bop[1] += gfxs->src_field_offset/2;
                         gfxs->Bop[2] += gfxs->src_field_offset/2;
                    }
                    else {
                         gfxs->Bop[1] += pitch/2 - gfxs->src_field_offset/2;
                         gfxs->Bop[2] += pitch/2 - gfxs->src_field_offset/2;
                    }
               }
               else {
                    gfxs->Bop[1] += pitch/2;
                    gfxs->Bop[2] += pitch/2;
               }
          }
          else if (gfxs->src_format == DSPF_NV12 || gfxs->src_format == DSPF_NV21) {
               if (gfxs->BopY & 1) {
                    if (gfxs->src_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Bop_field & 2)
                              gfxs->Bop[1] += gfxs->src_field_offset/2;
                         else
                              gfxs->Bop[1] += pitch - gfxs->src_field_offset/2;
                    }
                    else {
                         gfxs->Bop[1] += pitch;
                    }
               }
          }
          else if (gfxs->src_format == DSPF_YUV444P) {
               if (gfxs->src_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Bop_field & 1) {
                         gfxs->Bop[1] += gfxs->src_field_offset;
                         gfxs->Bop[2] += gfxs->src_field_offset;
                    }
                    else {
                         gfxs->Bop[1] += pitch - gfxs->src_field_offset;
                         gfxs->Bop[2] += pitch - gfxs->src_field_offset;
                    }
               }
               else {
                    gfxs->Bop[1] += pitch;
                    gfxs->Bop[2] += pitch;
               }
          }
          else { /* NV16 */
               if (gfxs->src_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Bop_field & 1)
                         gfxs->Bop[1] += gfxs->src_field_offset;
                    else
                         gfxs->Bop[1] += pitch - gfxs->src_field_offset;
               }
               else {
                    gfxs->Bop[1] += pitch;
               }
          }
     }

     gfxs->BopY++;
}

void Genefx_Bop_prev( GenefxState *gfxs )
{
     int pitch = gfxs->src_pitch;

     if (gfxs->src_caps & DSCAPS_SEPARATED) {
          gfxs->Bop_field++;

          if (gfxs->Bop_field & 1)
               gfxs->Bop[0] += gfxs->src_field_offset - pitch;
          else
               gfxs->Bop[0] -= gfxs->src_field_offset;
     }
     else
          gfxs->Bop[0] -= pitch;

     if (DFB_PLANAR_PIXELFORMAT(gfxs->src_format)) {
          if (gfxs->src_format == DSPF_YV12 || gfxs->src_format == DSPF_I420) {
               if (gfxs->BopY & 1) {
                    if (gfxs->src_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Bop_field & 2) {
                              gfxs->Bop[1] += gfxs->src_field_offset/4 - pitch/2;
                              gfxs->Bop[2] += gfxs->src_field_offset/4 - pitch/2;
                         }
                         else {
                              gfxs->Bop[1] -= gfxs->src_field_offset/4;
                              gfxs->Bop[2] -= gfxs->src_field_offset/4;
                         }
                    }
                    else {
                         gfxs->Bop[1] -= pitch/2;
                         gfxs->Bop[2] -= pitch/2;
                    }
               }
          }
          else if (gfxs->src_format == DSPF_YV16) {
               if (gfxs->src_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Bop_field & 2) {
                         gfxs->Bop[1] += gfxs->src_field_offset/2 - pitch/2;
                         gfxs->Bop[2] += gfxs->src_field_offset/2 - pitch/2;
                    }
                    else {
                         gfxs->Bop[1] -= gfxs->src_field_offset/2;
                         gfxs->Bop[2] -= gfxs->src_field_offset/2;
                    }
               }
               else {
                    gfxs->Bop[1] -= pitch/2;
                    gfxs->Bop[2] -= pitch/2;
               }
          }
          else if (gfxs->src_format == DSPF_NV12 || gfxs->src_format == DSPF_NV21) {
               if (gfxs->BopY & 1) {
                    if (gfxs->src_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Bop_field & 2)
                              gfxs->Bop[1] += gfxs->src_field_offset/2 - pitch;
                         else
                              gfxs->Bop[1] -= gfxs->src_field_offset/2;
                    }
                    else {
                         gfxs->Bop[1] -= pitch;
                    }
               }
          }
          else if (gfxs->src_format == DSPF_YUV444P) {
               if (gfxs->src_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Bop_field & 1) {
                         gfxs->Bop[1] += gfxs->src_field_offset - pitch;
                         gfxs->Bop[2] += gfxs->src_field_offset - pitch;
                    }
                    else {
                         gfxs->Bop[1] -= gfxs->src_field_offset;
                         gfxs->Bop[2] -= gfxs->src_field_offset;
                    }
               }
               else {
                    gfxs->Bop[1] -= pitch;
                    gfxs->Bop[2] -= pitch;
               }
          }
          else { /* NV16 */
               if (gfxs->src_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Bop_field & 1)
                         gfxs->Bop[1] += gfxs->src_field_offset - pitch;
                    else
                         gfxs->Bop[1] -= gfxs->src_field_offset;
               }
               else {
                    gfxs->Bop[1] -= pitch;
               }
          }
     }

     gfxs->BopY--;
}

/**********************************************************************************************************************/

void Genefx_Mop_xy( GenefxState *gfxs, int x, int y )
{
     int pitch = gfxs->mask_pitch;

     gfxs->Mop[0] = gfxs->mask_org[0];
     gfxs->MopY   = y;

     if (gfxs->mask_caps & DSCAPS_SEPARATED) {
          gfxs->Mop_field = y & 1;
          if (gfxs->Mop_field)
               gfxs->Mop[0] += gfxs->mask_field_offset;

          y /= 2;
     }

     D_ASSUME( !(x & DFB_PIXELFORMAT_ALIGNMENT(gfxs->mask_format)) );

     gfxs->Mop[0] += y * pitch + DFB_BYTES_PER_LINE( gfxs->mask_format, x );

     if (DFB_PLANAR_PIXELFORMAT(gfxs->mask_format)) {
          int mask_field_offset = gfxs->mask_field_offset;

          switch (gfxs->mask_format) {
               case DSPF_YV12:
               case DSPF_I420:
                    mask_field_offset /= 4;
                    pitch /= 2;
                    y /= 2;
                    x /= 2;
                    break;
               case DSPF_YV16:
                    mask_field_offset /= 2;
                    pitch /= 2;
                    x /= 2;
                    break;
               case DSPF_NV12:
               case DSPF_NV21:
                    mask_field_offset /= 2;
                    y /= 2;
               case DSPF_NV16:
                    x &= ~1;
                    break;
               case DSPF_YUV444P: /* nothing to adjust */
               default:
                    break;
          }

          gfxs->Mop[1] = gfxs->mask_org[1];
          gfxs->Mop[2] = gfxs->mask_org[2];

          if (gfxs->mask_caps & DSCAPS_SEPARATED && gfxs->Mop_field) {
               gfxs->Mop[1] += mask_field_offset;
               gfxs->Mop[2] += mask_field_offset;
          }

          gfxs->Mop[1] += y * pitch + x;
          gfxs->Mop[2] += y * pitch + x;
     }
}

void Genefx_Mop_next( GenefxState *gfxs )
{
     int pitch = gfxs->mask_pitch;

     if (gfxs->mask_caps & DSCAPS_SEPARATED) {
          gfxs->Mop_field++;

          if (gfxs->Mop_field & 1)
               gfxs->Mop[0] += gfxs->mask_field_offset;
          else
               gfxs->Mop[0] += pitch - gfxs->mask_field_offset;
     }
     else
          gfxs->Mop[0] += pitch;

     if (DFB_PLANAR_PIXELFORMAT(gfxs->mask_format)) {
          if (gfxs->mask_format == DSPF_YV12 || gfxs->mask_format == DSPF_I420) {
               if (gfxs->MopY & 1) {
                    if (gfxs->mask_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Mop_field & 2) {
                              gfxs->Mop[1] += gfxs->mask_field_offset/4;
                              gfxs->Mop[2] += gfxs->mask_field_offset/4;
                         }
                         else {
                              gfxs->Mop[1] += pitch/2 - gfxs->mask_field_offset/4;
                              gfxs->Mop[2] += pitch/2 - gfxs->mask_field_offset/4;
                         }
                    }
                    else {
                         gfxs->Mop[1] += pitch/2;
                         gfxs->Mop[2] += pitch/2;
                    }
               }
          }
          else if (gfxs->mask_format == DSPF_YV16) {
               if (gfxs->mask_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Mop_field & 2) {
                         gfxs->Mop[1] += gfxs->mask_field_offset/2;
                         gfxs->Mop[2] += gfxs->mask_field_offset/2;
                    }
                    else {
                         gfxs->Mop[1] += pitch/2 - gfxs->mask_field_offset/2;
                         gfxs->Mop[2] += pitch/2 - gfxs->mask_field_offset/2;
                    }
               }
               else {
                    gfxs->Mop[1] += pitch/2;
                    gfxs->Mop[2] += pitch/2;
               }
          }
          else if (gfxs->mask_format == DSPF_NV12 || gfxs->mask_format == DSPF_NV21) {
               if (gfxs->MopY & 1) {
                    if (gfxs->mask_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Mop_field & 2)
                              gfxs->Mop[1] += gfxs->mask_field_offset/2;
                         else
                              gfxs->Mop[1] += pitch - gfxs->mask_field_offset/2;
                    }
                    else {
                         gfxs->Mop[1] += pitch;
                    }
               }
          }
          else if (gfxs->mask_format == DSPF_YUV444P) {
               if (gfxs->mask_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Mop_field & 1) {
                         gfxs->Mop[1] += gfxs->mask_field_offset;
                         gfxs->Mop[2] += gfxs->mask_field_offset;
                    }
                    else {
                         gfxs->Mop[1] += pitch - gfxs->mask_field_offset;
                         gfxs->Mop[2] += pitch - gfxs->mask_field_offset;
                    }
               }
               else {
                    gfxs->Mop[1] += pitch;
                    gfxs->Mop[2] += pitch;
               }
          }
          else { /* NV16 */
               if (gfxs->mask_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Mop_field & 1)
                         gfxs->Mop[1] += gfxs->mask_field_offset;
                    else
                         gfxs->Mop[1] += pitch - gfxs->mask_field_offset;
               }
               else {
                    gfxs->Mop[1] += pitch;
               }
          }
     }

     gfxs->MopY++;
}

void Genefx_Mop_prev( GenefxState *gfxs )
{
     int pitch = gfxs->mask_pitch;

     if (gfxs->mask_caps & DSCAPS_SEPARATED) {
          gfxs->Mop_field++;

          if (gfxs->Mop_field & 1)
               gfxs->Mop[0] += gfxs->mask_field_offset - pitch;
          else
               gfxs->Mop[0] -= gfxs->mask_field_offset;
     }
     else
          gfxs->Mop[0] -= pitch;

     if (DFB_PLANAR_PIXELFORMAT(gfxs->mask_format)) {
          if (gfxs->mask_format == DSPF_YV12 || gfxs->mask_format == DSPF_I420) {
               if (gfxs->MopY & 1) {
                    if (gfxs->mask_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Mop_field & 2) {
                              gfxs->Mop[1] += gfxs->mask_field_offset/4 - pitch/2;
                              gfxs->Mop[2] += gfxs->mask_field_offset/4 - pitch/2;
                         }
                         else {
                              gfxs->Mop[1] -= gfxs->mask_field_offset/4;
                              gfxs->Mop[2] -= gfxs->mask_field_offset/4;
                         }
                    }
                    else {
                         gfxs->Mop[1] -= pitch/2;
                         gfxs->Mop[2] -= pitch/2;
                    }
               }
          }
          else if (gfxs->mask_format == DSPF_YV16) {
               if (gfxs->mask_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Mop_field & 2) {
                         gfxs->Mop[1] += gfxs->mask_field_offset/2 - pitch/2;
                         gfxs->Mop[2] += gfxs->mask_field_offset/2 - pitch/2;
                    }
                    else {
                         gfxs->Mop[1] -= gfxs->mask_field_offset/2;
                         gfxs->Mop[2] -= gfxs->mask_field_offset/2;
                    }
               }
               else {
                    gfxs->Mop[1] -= pitch/2;
                    gfxs->Mop[2] -= pitch/2;
               }
          }
          else if (gfxs->mask_format == DSPF_NV12 || gfxs->mask_format == DSPF_NV21) {
               if (gfxs->MopY & 1) {
                    if (gfxs->mask_caps & DSCAPS_SEPARATED) {
                         if (gfxs->Mop_field & 2)
                              gfxs->Mop[1] += gfxs->mask_field_offset/2 - pitch;
                         else
                              gfxs->Mop[1] -= gfxs->mask_field_offset/2;
                    }
                    else {
                         gfxs->Mop[1] -= pitch;
                    }
               }
          }
          else if (gfxs->mask_format == DSPF_YUV444P) {
               if (gfxs->mask_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Mop_field & 1) {
                         gfxs->Mop[1] += gfxs->mask_field_offset - pitch;
                         gfxs->Mop[2] += gfxs->mask_field_offset - pitch;
                    }
                    else {
                         gfxs->Mop[1] -= gfxs->mask_field_offset;
                         gfxs->Mop[2] -= gfxs->mask_field_offset;
                    }
               }
               else {
                    gfxs->Mop[1] -= pitch;
                    gfxs->Mop[2] -= pitch;
               }
          }
          else { /* NV16 */
               if (gfxs->mask_caps & DSCAPS_SEPARATED) {
                    if (gfxs->Mop_field & 1)
                         gfxs->Mop[1] += gfxs->mask_field_offset - pitch;
                    else
                         gfxs->Mop[1] -= gfxs->mask_field_offset;
               }
               else {
                    gfxs->Mop[1] -= pitch;
               }
          }
     }

     gfxs->MopY--;
}

/**********************************************************************************************************************/

bool
Genefx_ABacc_prepare( GenefxState *gfxs, int width )
{
     int size;

     if (!gfxs->need_accumulator)
          return true;

     size = (width + 31) & ~31;

     if (gfxs->ABsize < size) {
          void *ABstart = D_MALLOC( size * sizeof(GenefxAccumulator) * 3 + 31 );

          if (!ABstart) {
               D_WARN( "out of memory" );
               return false;
          }

          if (gfxs->ABstart)
               D_FREE( gfxs->ABstart );

          gfxs->ABstart = ABstart;
          gfxs->ABsize  = size;
          gfxs->Aacc    = (GenefxAccumulator*) (((unsigned long)ABstart+31) & ~31);
          gfxs->Bacc    = gfxs->Aacc + size;
          gfxs->Tacc    = gfxs->Aacc + size + size;
     }

     gfxs->Sacc = gfxs->Dacc = gfxs->Aacc;

     return true;
}

void
Genefx_ABacc_flush( GenefxState *gfxs )
{
     if (dfb_config->keep_accumulators >= 0 && gfxs->ABsize > dfb_config->keep_accumulators) {
          D_FREE( gfxs->ABstart );

          gfxs->ABsize  = 0;
          gfxs->ABstart = NULL;
          gfxs->Aacc    = NULL;
          gfxs->Bacc    = NULL;
          gfxs->Sacc    = NULL;
          gfxs->Dacc    = NULL;
     }
}

