/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI Radeon cards written by
 *             Claudio Ciccani <klan@users.sf.net>.  
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dfb_types.h>
#include <directfb.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/state.h>
#include <core/gfxcard.h>

#include "radeon.h"
#include "radeon_regs.h"
#include "radeon_mmio.h"
#include "radeon_2d.h"


static void
radeonDoFillRectangle2D( RadeonDriverData *rdrv,
                         RadeonDeviceData *rdev,
                         DFBRectangle     *rect )
{
     volatile u8 *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 2 );
     
     radeon_out32( mmio, DST_Y_X, (rect->y << 16) |
                                (rect->x & 0x3fff) );
     radeon_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) |
                                         (rect->w & 0x3fff) );
}

bool radeonFillRectangle2D( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     if (rdev->dst_422) {
          rect->x /= 2;
          rect->w  = (rect->w+1) >> 1;
     }

     radeonDoFillRectangle2D( rdrv, rdev, rect );

     return true;
}

bool radeonFillRectangle2D_420( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     DFBRegion        *clip = &rdev->clip;
     volatile u8      *mmio = rdrv->mmio_base;

     /* Fill Luma plane */
     radeonDoFillRectangle2D( rdrv, rdev, rect );

     /* Scale coordinates */
     rect->x /= 2;
     rect->y /= 2;
     rect->w  = (rect->w+1) >> 1;
     rect->h  = (rect->h+1) >> 1;

     /* Prepare Cb plane */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset_cb );
     radeon_out32( mmio, DST_PITCH, rdev->dst_pitch/2 );
     radeon_out32( mmio, SC_TOP_LEFT, (clip->y1/2 << 16) |
                                      (clip->x1/2 & 0xffff) );
     radeon_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1)/2 << 16) |
                                          ((clip->x2+1)/2 & 0xffff) );
     radeon_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cb_cop );

     /* Fill Cb plane */
     radeonDoFillRectangle2D( rdrv, rdev, rect );
     
     /* Prepare Cr plane */
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset_cr );
     radeon_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cr_cop );

     /* Fill Cr plane */
     radeonDoFillRectangle2D( rdrv, rdev, rect );

     /* Reset */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset );
     radeon_out32( mmio, DST_PITCH, rdev->dst_pitch );
     radeon_out32( mmio, SC_TOP_LEFT, (clip->y1 << 16) |
                                      (clip->x1 & 0xffff) );
     radeon_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1) << 16) |
                                          ((clip->x2+1) & 0xffff) );
     radeon_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->y_cop );
          
     return true;
}

static void
radeonDoDrawRectangle2D( RadeonDriverData *rdrv,
                         RadeonDeviceData *rdev,
                         DFBRectangle     *rect )
{
     volatile u8 *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 7 );
     
     /* left line */
     radeon_out32( mmio, DST_Y_X, (rect->y << 16) | (rect->x & 0x3fff) );
     radeon_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) | 1 );
     /* top line */
     radeon_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | (rect->w & 0xffff) );
     /* bottom line */
     radeon_out32( mmio, DST_Y_X, ((rect->y+rect->h-1) << 16) | (rect->x & 0x3fff) );
     radeon_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | (rect->w & 0xffff) );
     /* right line */
     radeon_out32( mmio, DST_Y_X, (rect->y << 16) | ((rect->x+rect->w-1) & 0x3fff) );
     radeon_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) | 1 );
}

bool radeonDrawRectangle2D( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     if (rdev->dst_422) {
          rect->x /= 2;
          rect->w  = (rect->w+1) >> 1;
     }

     radeonDoDrawRectangle2D( rdrv, rdev, rect );

     return true;
}

bool radeonDrawRectangle2D_420( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     DFBRegion        *clip = &rdev->clip;
     volatile u8      *mmio = rdrv->mmio_base;

     /* Fill Luma plane */
     radeonDoDrawRectangle2D( rdrv, rdev, rect );

     /* Scale coordinates */
     rect->x  /= 2;
     rect->y  /= 2;
     rect->w >>= 1;
     rect->h >>= 1;

     /* Prepare Cb plane */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset_cb );
     radeon_out32( mmio, DST_PITCH, rdev->dst_pitch/2 );
     radeon_out32( mmio, SC_TOP_LEFT, (clip->y1/2 << 16) |
                                      (clip->x1/2 & 0xffff) );
     radeon_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1)/2 << 16) |
                                          ((clip->x2+1)/2 & 0xffff) );
     radeon_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cb_cop );

     /* Fill Cb plane */
     radeonDoDrawRectangle2D( rdrv, rdev, rect );
     
     /* Prepare Cr plane */
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset_cr );
     radeon_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cr_cop );

     /* Fill Cr plane */
     radeonDoDrawRectangle2D( rdrv, rdev, rect );

     /* Reset */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset );
     radeon_out32( mmio, DST_PITCH, rdev->dst_pitch );
     radeon_out32( mmio, SC_TOP_LEFT, (clip->y1 << 16) |
                                      (clip->x1 & 0xffff) );
     radeon_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1) << 16) |
                                          ((clip->x2+1) & 0xffff) );
     radeon_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->y_cop );
     
     return true;
}

static void
radeonDoDrawLine2D( RadeonDriverData *rdrv,
                    RadeonDeviceData *rdev,
                    DFBRegion        *line )
{
     volatile u8 *mmio = rdrv->mmio_base;

     radeon_waitfifo( rdrv, rdev, 2 );
     
     radeon_out32( mmio, DST_LINE_START, (line->y1 << 16) | 
                                       (line->x1 & 0xffff) );
     radeon_out32( mmio, DST_LINE_END, (line->y2 << 16) |
                                     (line->x2 & 0xffff) );
}

bool radeonDrawLine2D( void *drv, void *dev, DFBRegion *line )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     if (rdev->dst_422) {
          line->x1 /= 2;
          line->x2  = (line->x2+1) / 2;
     }
     
     radeonDoDrawLine2D( rdrv, rdev, line );

     return true;
}

bool radeonDrawLine2D_420( void *drv, void *dev, DFBRegion *line )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     DFBRegion        *clip = &rdev->clip;
     volatile u8      *mmio = rdrv->mmio_base;
     
     line->x1 &= ~1;
     line->y1 &= ~1;
     line->x2 &= ~1;
     line->y2 &= ~1;

     /* Fill Luma plane */
     radeonDoDrawLine2D( rdrv, rdev, line );

     /* Scale coordinates */
     line->x1 /= 2;
     line->y1 /= 2;
     line->x2 /= 2;
     line->y2 /= 2;
          
     /* Prepare Cb plane */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset_cb );
     radeon_out32( mmio, DST_PITCH, rdev->dst_pitch/2 );
     radeon_out32( mmio, SC_TOP_LEFT, (clip->y1/2 << 16) |
                                      (clip->x1/2 & 0xffff) );
     radeon_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1)/2 << 16) |
                                          ((clip->x2+1)/2 & 0xffff) );
     radeon_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cb_cop );

     /* Fill Cb plane */
     radeonDoDrawLine2D( rdrv, rdev, line );
     
     /* Prepare Cr plane */
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset_cr );
     radeon_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cr_cop );

     /* Fill Cr plane */
     radeonDoDrawLine2D( rdrv, rdev, line );

     /* Reset */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset );
     radeon_out32( mmio, DST_PITCH, rdev->dst_pitch );
     radeon_out32( mmio, SC_TOP_LEFT, (clip->y1 << 16) |
                                      (clip->x1 & 0xffff) );
     radeon_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1) << 16) |
                                          ((clip->x2+1) & 0xffff) );
     radeon_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->y_cop );
          
     return true;
}

static void 
radeonDoBlit2D( RadeonDriverData *rdrv, RadeonDeviceData *rdev,
                int sx, int sy, int dx, int dy, int w, int h )
{
     volatile u8   *mmio = rdrv->mmio_base;
     u32            dir  = 0;
     
     /* check which blitting direction should be used */
     if (sx <= dx) {
          sx += w-1;
          dx += w-1;
     } else
          dir |= DST_X_LEFT_TO_RIGHT;

     if (sy <= dy) {
          sy += h-1;
          dy += h-1;
     } else
          dir |= DST_Y_TOP_TO_BOTTOM;

     radeon_waitfifo( rdrv, rdev, 4 ); 
     
     radeon_out32( mmio, DP_CNTL, dir ); 
     radeon_out32( mmio, SRC_Y_X,          (sy << 16) | (sx & 0x3fff) );
     radeon_out32( mmio, DST_Y_X,          (dy << 16) | (dx & 0x3fff) );
     radeon_out32( mmio, DST_HEIGHT_WIDTH, (h  << 16) | (w  & 0x3fff) );
}

bool radeonBlit2D( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     if (rdev->dst_422) {
          sr->x /= 2;
          sr->w  = (sr->w+1) >> 1;
          dx    /= 2;
     }

     radeonDoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );

     return true;
}

bool radeonBlit2D_420( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;  
     DFBRegion        *clip = &rdev->clip;
     volatile u8      *mmio = rdrv->mmio_base;

     /* Blit Luma plane */
     radeonDoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );

     /* Scale coordinates */
     sr->x /= 2;
     sr->y /= 2;
     sr->w  = (sr->w+1) >> 1;
     sr->h  = (sr->h+1) >> 1;
     dx    /= 2;
     dy    /= 2;
     
     /* Prepare Cb plane */
     radeon_waitfifo( rdrv, rdev, 6 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset_cb );
     radeon_out32( mmio, DST_PITCH, rdev->dst_pitch/2 );
     radeon_out32( mmio, SRC_OFFSET, rdev->src_offset_cb );
     radeon_out32( mmio, SRC_PITCH, rdev->src_pitch/2 );
     radeon_out32( mmio, SC_TOP_LEFT, (clip->y1/2 << 16) |
                                    (clip->x1/2 & 0xffff) );
     radeon_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1/2) << 16) |
                                        ((clip->x2+1/2) & 0xffff) );

     /* Blit Cb plane */
     radeonDoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );
     
     /* Prepare Cr plane */
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset_cr );
     radeon_out32( mmio, SRC_OFFSET, rdev->src_offset_cr );

     /* Blit Cr plane */
     radeonDoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );
     
     /* Reset */
     radeon_waitfifo( rdrv, rdev, 6 );
     radeon_out32( mmio, DST_OFFSET, rdev->dst_offset );
     radeon_out32( mmio, DST_PITCH, rdev->dst_pitch );
     radeon_out32( mmio, SRC_OFFSET, rdev->src_offset );
     radeon_out32( mmio, SRC_PITCH, rdev->src_pitch );
     radeon_out32( mmio, SC_TOP_LEFT, (clip->y1 << 16) |
                                    (clip->x1 & 0xffff) );
     radeon_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1) << 16) |
                                        ((clip->x2+1) & 0xffff) );

     return true;
}

