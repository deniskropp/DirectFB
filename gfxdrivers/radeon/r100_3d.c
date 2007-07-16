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
#include "radeon_3d.h"


static __inline__ void
out_vertex2d0( volatile u8 *mmio, float x, float y )
{
     radeon_out32( mmio, SE_PORT_DATA0, f2d(x) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(y) );
}

static __inline__ void
out_vertex2d2( volatile u8 *mmio, float x, float y, float s, float t )
{
     radeon_out32( mmio, SE_PORT_DATA0, f2d(x) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(y) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(s) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(t) );
}

static __inline__ void
out_vertex3d( volatile u8 *mmio,
              float x, float y, float z, float w, float s, float t )
{
     radeon_out32( mmio, SE_PORT_DATA0, f2d(x) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(y) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(z) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(w) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(s) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(t) );
}


static void
r100DoFillRectangle3D( RadeonDriverData *rdrv,
                       RadeonDeviceData *rdev,
                       DFBRectangle     *rect )
{
     volatile u8 *mmio = rdrv->mmio_base;

     if (rect->w == 1 && rect->h == 1) {
          radeon_waitfifo( rdrv, rdev, 3 );
          
          radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_POINT_LIST |
                                          VF_PRIM_WALK_DATA       |
                                          VF_RADEON_MODE          |
                                          (1 << VF_NUM_VERTICES_SHIFT) );

          out_vertex2d0( mmio, rect->x+1, rect->y+1 );
     }
     else {
          radeon_waitfifo( rdrv, rdev, 7 );
     
          radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_RECTANGLE_LIST |
                                          VF_PRIM_WALK_DATA           |
                                          VF_RADEON_MODE              |
                                          (3 << VF_NUM_VERTICES_SHIFT) );

          out_vertex2d0( mmio, rect->x        , rect->y         );
          out_vertex2d0( mmio, rect->x+rect->w, rect->y         );
          out_vertex2d0( mmio, rect->x+rect->w, rect->y+rect->h );
     }
}

bool r100FillRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     r100DoFillRectangle3D( rdrv, rdev, rect );

     return true;
}

bool r100FillRectangle3D_420( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     DFBRegion        *clip = &rdev->clip;
     volatile u8      *mmio = rdrv->mmio_base;

     /* Fill Luma plane */
     r100DoFillRectangle3D( rdrv, rdev, rect );
          
     /* Scale coordinates */
     rect->x /= 2;
     rect->y /= 2;
     rect->w  = (rect->w+1) >> 1;
     rect->h  = (rect->h+1) >> 1;

     /* Prepare Cb plane */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                      (clip->x1/2 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) |
                                          (clip->x2/2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->cb_cop );

     /* Fill Cb plane */
     r100DoFillRectangle3D( rdrv, rdev, rect );

     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->cr_cop );

     /* Fill Cr plane */
     r100DoFillRectangle3D( rdrv, rdev, rect );

     /* Reset */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                      (clip->x1 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                          (clip->x2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->y_cop );

     return true;
}

static void
r100DoFillTriangle( RadeonDriverData *rdrv,
                    RadeonDeviceData *rdev,
                    DFBTriangle      *tri )
{
     volatile u8 *mmio = rdrv->mmio_base;

     radeon_waitfifo( rdrv, rdev, 7 );
     
     radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_TRIANGLE_LIST |
                                     VF_PRIM_WALK_DATA          |
                                     VF_RADEON_MODE             |
                                     (3 << VF_NUM_VERTICES_SHIFT) );
     
     out_vertex2d0( mmio, tri->x1, tri->y1 );
     out_vertex2d0( mmio, tri->x2, tri->y2 );
     out_vertex2d0( mmio, tri->x3, tri->y3 );
}

bool r100FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     r100DoFillTriangle( rdrv, rdev, tri );
     
     return true;
}

bool r100FillTriangle_420( void *drv, void *dev, DFBTriangle *tri )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     DFBRegion        *clip = &rdev->clip;
     volatile u8      *mmio = rdrv->mmio_base;

     /* Fill Luma plane */
     r100DoFillTriangle( rdrv, rdev, tri );
          
     /* Scale coordinates */
     tri->x1 /= 2;
     tri->y1 /= 2;
     tri->x2 /= 2;
     tri->y2 /= 2;
     tri->x3 /= 2;
     tri->y3 /= 2;

     /* Prepare Cb plane */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                      (clip->x1/2 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) |
                                          (clip->x2/2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->cb_cop );

     /* Fill Cb plane */
     r100DoFillTriangle( rdrv, rdev, tri );

     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->cr_cop );

     /* Fill Cr plane */
     r100DoFillTriangle( rdrv, rdev, tri );

     /* Reset */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                      (clip->x1 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                          (clip->x2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->y_cop );
     
     return true;
}

static void
r100DoDrawRectangle3D( RadeonDriverData *rdrv,
                       RadeonDeviceData *rdev,
                       DFBRectangle     *rect )
{
     volatile u8 *mmio = rdrv->mmio_base;

     radeon_waitfifo( rdrv, rdev, 25 );
          
     radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_RECTANGLE_LIST |
                                     VF_PRIM_WALK_DATA           |
                                     VF_RADEON_MODE              |
                                     (12 << VF_NUM_VERTICES_SHIFT) );
     /* top line */
     out_vertex2d0( mmio, rect->x          , rect->y           );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y           );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y+1         );
     /* right line */
     out_vertex2d0( mmio, rect->x+rect->w-1, rect->y+1         );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y+1         );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y+rect->h-1 );
     /* bottom line */
     out_vertex2d0( mmio, rect->x          , rect->y+rect->h-1 );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y+rect->h-1 );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y+rect->h   );
     /* left line */
     out_vertex2d0( mmio, rect->x          , rect->y+1         );
     out_vertex2d0( mmio, rect->x+1        , rect->y+1         );
     out_vertex2d0( mmio, rect->x+1        , rect->y+rect->h-1 );
}

bool r100DrawRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     r100DoDrawRectangle3D( rdrv, rdev, rect );

     return true;
}

bool r100DrawRectangle3D_420( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     DFBRegion        *clip = &rdev->clip;
     volatile u8      *mmio = rdrv->mmio_base;
     
     /* Fill Luma plane */
     r100DoDrawRectangle3D( rdrv, rdev, rect );
          
     /* Scale coordinates */
     rect->x  /= 2;
     rect->y  /= 2;
     rect->w >>= 1;
     rect->h >>= 1;

     /* Prepare Cb plane */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                      (clip->x1/2 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) |
                                          (clip->x2/2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->cb_cop );

     /* Fill Cb plane */
     r100DoDrawRectangle3D( rdrv, rdev, rect );

     /* Prepare Cr plane */
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->cr_cop );

     /* Fill Cr plane */
     r100DoDrawRectangle3D( rdrv, rdev, rect );

     /* Reset */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                      (clip->x1 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                          (clip->x2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->y_cop );

     return true;
}

static void
r100DoDrawLine3D( RadeonDriverData *rdrv,
                  RadeonDeviceData *rdev,
                  DFBRegion        *line )
{
     volatile u8 *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 5 );
     
     radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_LINE_LIST |
                                     VF_PRIM_WALK_DATA      |
                                     VF_RADEON_MODE         |
                                     (2 << VF_NUM_VERTICES_SHIFT) );

     out_vertex2d0( mmio, line->x1, line->y1 );
     out_vertex2d0( mmio, line->x2, line->y2 );
}

bool r100DrawLine3D( void *drv, void *dev, DFBRegion *line )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     r100DoDrawLine3D( rdrv, rdev, line );

     return true;
}

bool r100DrawLine3D_420( void *drv, void *dev, DFBRegion *line )
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
     r100DoDrawLine3D( rdrv, rdev, line );
          
     /* Scale coordinates */
     line->x1 /= 2;
     line->y1 /= 2;
     line->x2 /= 2;
     line->y2 /= 2;
          
     /* Prepare Cb plane */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                      (clip->x1/2 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) |
                                          (clip->x2/2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->cb_cop );

     /* Fill Cb plane */
     r100DoDrawLine3D( rdrv, rdev, line );

     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->cr_cop );

     /* Fill Cr plane */
     r100DoDrawLine3D( rdrv, rdev, line );

     /* Reset */
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                      (clip->x1 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                          (clip->x2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_1, rdev->y_cop );

     return true;
}

static void
r100DoBlit3D( RadeonDriverData *rdrv, RadeonDeviceData *rdev,
              DFBRectangle     *sr,   DFBRectangle     *dr )
{
     volatile u8 *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 13 );

     radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_RECTANGLE_LIST |
                                     VF_PRIM_WALK_DATA           |
                                     VF_RADEON_MODE              |
                                     (3 << VF_NUM_VERTICES_SHIFT) );
     
     if (rdev->blittingflags & DSBLIT_ROTATE180) {
          out_vertex2d2( mmio, dr->x      , dr->y      , sr->x+sr->w, sr->y+sr->h );
          out_vertex2d2( mmio, dr->x+dr->w, dr->y      , sr->x      , sr->y+sr->h );
          out_vertex2d2( mmio, dr->x+dr->w, dr->y+dr->h, sr->x      , sr->y       );
     }
     else {
          out_vertex2d2( mmio, dr->x      , dr->y      , sr->x      , sr->y       );
          out_vertex2d2( mmio, dr->x+dr->w, dr->y      , sr->x+sr->w, sr->y       );
          out_vertex2d2( mmio, dr->x+dr->w, dr->y+dr->h, sr->x+sr->w, sr->y+sr->h );
     }
}

bool r100Blit3D( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     DFBRectangle      dr   = { dx, dy, sr->w, sr->h };

     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }
          
     r100DoBlit3D( rdrv, rdev, sr, &dr );

     return true;
}

bool r100Blit3D_420( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     DFBRectangle dr = { dx, dy, sr->w, sr->h };
          
     return r100StretchBlit_420( drv, dev, sr, &dr );
}

bool r100StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     
     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }
     
     r100DoBlit3D( rdrv, rdev, sr, dr );
     
     return true;
}

bool r100StretchBlit_420( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     RadeonDriverData *rdrv    = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev    = (RadeonDeviceData*) dev; 
     DFBRegion        *clip    = &rdev->clip;
     volatile u8      *mmio    = rdrv->mmio_base;
     bool              src_420 = DFB_PLANAR_PIXELFORMAT( rdev->src_format );
     
     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }

     /* Blit Luma plane */
     r100DoBlit3D( rdrv, rdev, sr, dr );

     /* Scale coordinates */
     if (src_420) {
          sr->x /= 2;
          sr->y /= 2;
          sr->w  = (sr->w+1) >> 1;
          sr->h  = (sr->h+1) >> 1;
     }
     dr->x /= 2;
     dr->y /= 2;
     dr->w  = (dr->w+1) >> 1;
     dr->h  = (dr->h+1) >> 1;

     /* Prepare Cb plane */
     radeon_waitfifo( rdrv, rdev, src_420 ? 8 : 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
     if (src_420) {
          radeon_out32( mmio, PP_TEX_SIZE_0, ((rdev->src_height/2-1) << 16) |
                                             ((rdev->src_width/2-1) & 0xffff) );
          radeon_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch/2 - 32 );
          radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cb );
     }
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                      (clip->x1/2 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) | 
                                          (clip->x2/2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_0, rdev->cb_cop );

     /* Blit Cb plane */
     r100DoBlit3D( rdrv, rdev, sr, dr );

     /* Prepare Cr plane */
     radeon_waitfifo( rdrv, rdev, src_420 ? 3 : 2 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     if (src_420)
          radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cr );
     radeon_out32( mmio, PP_TFACTOR_0, rdev->cr_cop );

     /* Blit Cr plane */
     r100DoBlit3D( rdrv, rdev, sr, dr );
          
     /* Reset */
     radeon_waitfifo( rdrv, rdev, src_420 ? 8 : 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     if (src_420) {
          radeon_out32( mmio, PP_TEX_SIZE_0, ((rdev->src_height-1) << 16) |
                                             ((rdev->src_width-1) & 0xffff) );
          radeon_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch - 32 );
          radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset );
     }
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                      (clip->x1 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                          (clip->x2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_0, rdev->y_cop );
     
     return true;
}

static void
r100DoTextureTriangles( RadeonDriverData *rdrv, RadeonDeviceData *rdev,
                        DFBVertex *ve, int num, u32 primitive )
{
     volatile u8   *mmio = rdrv->mmio_base;
     int            i;
 
     radeon_waitfifo( rdrv, rdev, 1 ); 
     
     radeon_out32( mmio, SE_VF_CNTL, primitive         | 
                                     VF_PRIM_WALK_DATA |
                                     VF_RADEON_MODE    |
                                     (num << VF_NUM_VERTICES_SHIFT) );

     for (; num >= 10; num -= 10) {
          radeon_waitfifo( rdrv, rdev, 60 );
          for (i = 0; i < 10; i++)
               out_vertex3d( mmio, ve[i].x, ve[i].y, ve[i].z,
                                   ve[i].w, ve[i].s, ve[i].t );
          ve += 10;
     }

     if (num > 0) {
          radeon_waitfifo( rdrv, rdev, num*6 );
          for (i = 0; i < num; i++)
               out_vertex3d( mmio, ve[i].x, ve[i].y, ve[i].z,
                                   ve[i].w, ve[i].s, ve[i].t );
     }
}

bool r100TextureTriangles( void *drv, void *dev, DFBVertex *ve,
                           int num, DFBTriangleFormation formation )
{ 
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     u32               prim = 0;

     if (num > 65535) {
          D_WARN( "R100 supports maximum 65535 vertices" );
          return false;
     }

     switch (formation) {
          case DTTF_LIST:
               prim = VF_PRIM_TYPE_TRIANGLE_LIST;
               break;
          case DTTF_STRIP:
               prim = VF_PRIM_TYPE_TRIANGLE_STRIP;
               break;
          case DTTF_FAN:
               prim = VF_PRIM_TYPE_TRIANGLE_FAN;
               break;
          default:
               D_BUG( "unexpected triangle formation" );
               return false;
     }
     
     r100DoTextureTriangles( rdrv, rdev, ve, num, prim );

     return true;
}

bool r100TextureTriangles_420( void *drv, void *dev, DFBVertex *ve,
                               int num, DFBTriangleFormation formation )
{ 
     RadeonDriverData *rdrv    = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev    = (RadeonDeviceData*) dev;
     DFBRegion        *clip    = &rdev->clip;
     volatile u8      *mmio    = rdrv->mmio_base;
     bool              src_420 = DFB_PLANAR_PIXELFORMAT( rdev->src_format );
     u32               prim    = 0;
     int               i;

     if (num > 65535) {
          D_WARN( "R100 supports maximum 65535 vertices" );
          return false;
     }

     switch (formation) {
          case DTTF_LIST:
               prim = VF_PRIM_TYPE_TRIANGLE_LIST;
               break;
          case DTTF_STRIP:
               prim = VF_PRIM_TYPE_TRIANGLE_STRIP;
               break;
          case DTTF_FAN:
               prim = VF_PRIM_TYPE_TRIANGLE_FAN;
               break;
          default:
               D_BUG( "unexpected triangle formation" );
               return false;
     }

     /* Map Luma plane */
     r100DoTextureTriangles( rdrv, rdev, ve, num, prim );

     /* Scale coordinates */
     for (i = 0; i < num; i++) {
          ve[i].x *= 0.5;
          ve[i].y *= 0.5;
     }

     /* Prepare Cb plane */
     radeon_waitfifo( rdrv, rdev, src_420 ? 8 : 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
     if (src_420) {
          radeon_out32( mmio, PP_TEX_SIZE_0, ((rdev->src_height/2-1) << 16) |
                                             ((rdev->src_width/2-1) & 0xffff) );
          radeon_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch/2 - 32 );
          radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cb );
     }
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                      (clip->x1/2 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) | 
                                          (clip->x2/2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_0, rdev->cb_cop );
     
     /* Map Cb plane */
     r100DoTextureTriangles( rdrv, rdev, ve, num, prim );
     
     /* Prepare Cr plane */
     radeon_waitfifo( rdrv, rdev, src_420 ? 3 : 2 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     if (src_420)
          radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cr );
     radeon_out32( mmio, PP_TFACTOR_0, rdev->cr_cop );

     /* Map Cr plane */
     r100DoTextureTriangles( rdrv, rdev, ve, num, prim );
     
     /* Reset */
     radeon_waitfifo( rdrv, rdev, src_420 ? 8 : 5 );
     radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     if (src_420) {
          radeon_out32( mmio, PP_TEX_SIZE_0, ((rdev->src_height-1) << 16) |
                                             ((rdev->src_width-1) & 0xffff) );
          radeon_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch - 32 );
          radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset );
     }
     radeon_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                      (clip->x1 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                          (clip->x2 & 0xffff) );
     radeon_out32( mmio, PP_TFACTOR_0, rdev->y_cop );
     
     return true;
}
