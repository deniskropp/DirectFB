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


#define EMIT_VERTICES( rdrv, rdev, mmio ) { \
     u32 *_v = (rdev)->vb; \
     u32  _s = (rdev)->vb_size; \
     radeon_waitfifo( rdrv, rdev, 1 ); \
     radeon_out32( mmio, SE_VF_CNTL, rdev->vb_type | VF_PRIM_WALK_DATA | VF_RADEON_MODE | \
                                    (rdev->vb_count << VF_NUM_VERTICES_SHIFT) ); \
     do { \
          u32 _n = MIN(_s, 64); \
          _s -= _n; \
          radeon_waitfifo( rdrv, rdev, _n ); \
          while (_n--) \
               radeon_out32( mmio, SE_PORT_DATA0, *_v++ ); \
     } while (_s); \
}

static void
r100_flush_vb( RadeonDriverData *rdrv, RadeonDeviceData *rdev )
{
     volatile u8 *mmio = rdrv->mmio_base;
    
     EMIT_VERTICES( rdrv, rdev, mmio );

     if (DFB_PLANAR_PIXELFORMAT(rdev->dst_format)) {
          DFBRegion *clip = &rdev->clip;
          bool       s420 = DFB_PLANAR_PIXELFORMAT(rdev->src_format);
          int        i;
          
          if (DFB_BLITTING_FUNCTION(rdev->accel)) {
               for (i = 0; i < rdev->vb_size; i += 4) {
                    rdev->vb[i+0] = f2d(d2f(rdev->vb[i+0])*0.5f);
                    rdev->vb[i+1] = f2d(d2f(rdev->vb[i+1])*0.5f);
                    if (s420) {
                         rdev->vb[i+2] = f2d(d2f(rdev->vb[i+2])*0.5f);
                         rdev->vb[i+3] = f2d(d2f(rdev->vb[i+3])*0.5f);
                    }
               }
          } else {
               for (i = 0; i < rdev->vb_size; i += 2) {
                    rdev->vb[i+0] = f2d(d2f(rdev->vb[i+0])*0.5f);
                    rdev->vb[i+1] = f2d(d2f(rdev->vb[i+1])*0.5f);
               }
          }

          /* Prepare Cb plane */
          radeon_waitfifo( rdrv, rdev, 5 );
          radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
          radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
          radeon_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                           (clip->x1/2 & 0xffff) );
          radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) |
                                               (clip->x2/2 & 0xffff) );
          if (DFB_BLITTING_FUNCTION(rdev->accel)) {
               radeon_out32( mmio, PP_TFACTOR_0, rdev->cb_cop );
               if (s420) {
                    radeon_waitfifo( rdrv, rdev, 3 );
                    radeon_out32( mmio, PP_TEX_SIZE_0, ((rdev->src_height/2-1) << 16) |
                                                       ((rdev->src_width/2-1) & 0xffff) );
                    radeon_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch/2 - 32 );
                    radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cb );
               }
          } else {
               radeon_out32( mmio, PP_TFACTOR_1, rdev->cb_cop );
          }

          /* Fill Cb plane */
          EMIT_VERTICES( rdrv, rdev, mmio );

          /* Prepare Cr plane */
          radeon_waitfifo( rdrv, rdev, 2 );
          radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
          if (DFB_BLITTING_FUNCTION(rdev->accel)) {
               radeon_out32( mmio, PP_TFACTOR_0, rdev->cr_cop );
               if (s420) {
                    radeon_waitfifo( rdrv, rdev, 1 );
                    radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cr );
               }
          } else {
               radeon_out32( mmio, PP_TFACTOR_1, rdev->cr_cop );
          }

          /* Fill Cr plane */
          EMIT_VERTICES( rdrv, rdev, mmio );

          /* Reset */
          radeon_waitfifo( rdrv, rdev, 5 );
          radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
          radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
          radeon_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                           (clip->x1 & 0xffff) );
          radeon_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                               (clip->x2 & 0xffff) );
          if (DFB_BLITTING_FUNCTION(rdev->accel)) {
               radeon_out32( mmio, PP_TFACTOR_0, rdev->y_cop );
               if (s420) {
                    radeon_waitfifo( rdrv, rdev, 3 );
                    radeon_out32( mmio, PP_TEX_SIZE_0, ((rdev->src_height-1) << 16) |
                                                       ((rdev->src_width-1) & 0xffff) );
                    radeon_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch - 32 );
                    radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset );
               }
          } else {
               radeon_out32( mmio, PP_TFACTOR_1, rdev->y_cop );
          }
     }
    
     rdev->vb_size  = 0;
     rdev->vb_count = 0;
}

static inline u32*
r100_init_vb( RadeonDriverData *rdrv, RadeonDeviceData *rdev, u32 type, u32 count, u32 size )
{
     u32 *vb;
    
     if (rdev->vb_size+size > sizeof(rdev->vb)/sizeof(rdev->vb[0]))
          r100_flush_vb( rdrv, rdev );
        
     vb = &rdev->vb[rdev->vb_size];
     rdev->vb_type   = type;
     rdev->vb_size  += size;
     rdev->vb_count += count;
    
     return vb;
}


bool r100FillRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     u32              *v;
     
     v = r100_init_vb( rdrv, rdev, VF_PRIM_TYPE_RECTANGLE_LIST, 3, 6 );
     *v++ = f2d(rect->x);         *v++ = f2d(rect->y);
     *v++ = f2d(rect->x+rect->w); *v++ = f2d(rect->y);
     *v++ = f2d(rect->x+rect->w); *v++ = f2d(rect->y+rect->h);
     
     return true;
}

bool r100FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     u32              *v;

     v = r100_init_vb( rdrv, rdev, VF_PRIM_TYPE_TRIANGLE_LIST, 3, 6 );
     *v++ = f2d(tri->x1); *v++ = f2d(tri->y1);
     *v++ = f2d(tri->x2); *v++ = f2d(tri->y2);
     *v++ = f2d(tri->x3); *v++ = f2d(tri->y3);
     
     return true;
}

bool r100DrawRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     u32              *v;

     v = r100_init_vb( rdrv, rdev, VF_PRIM_TYPE_RECTANGLE_LIST, 12, 24 );
     /* top line */
     *v++ = f2d(rect->x);           *v++ = f2d(rect->y);
     *v++ = f2d(rect->x+rect->w);   *v++ = f2d(rect->y);
     *v++ = f2d(rect->x+rect->w);   *v++ = f2d(rect->y+1);
     /* right line */
     *v++ = f2d(rect->x+rect->w-1); *v++ = f2d(rect->y+1);
     *v++ = f2d(rect->x+rect->w);   *v++ = f2d(rect->y+1);
     *v++ = f2d(rect->x+rect->w);   *v++ = f2d(rect->y+rect->h-1);
     /* bottom line */
     *v++ = f2d(rect->x);           *v++ = f2d(rect->y+rect->h-1);
     *v++ = f2d(rect->x+rect->w);   *v++ = f2d(rect->y+rect->h-1);
     *v++ = f2d(rect->x+rect->w);   *v++ = f2d(rect->y+rect->h);
     /* left line */
     *v++ = f2d(rect->x);           *v++ = f2d(rect->y+1);
     *v++ = f2d(rect->x+1);         *v++ = f2d(rect->y+1);
     *v++ = f2d(rect->x+1);         *v++ = f2d(rect->y+rect->h-1);
     
     return true;
}

bool r100DrawLine3D( void *drv, void *dev, DFBRegion *line )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     u32              *v;

     v = r100_init_vb( rdrv, rdev, VF_PRIM_TYPE_LINE_LIST, 2, 4 );
     *v++ = f2d(line->x1); *v++ = f2d(line->y1);
     *v++ = f2d(line->x2); *v++ = f2d(line->y2);

     return true;
}

bool r100Blit3D( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     DFBRectangle dr = { dx, dy, sr->w, sr->h };
     
     return r100StretchBlit( drv, dev, sr, &dr );
}

bool r100StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     u32              *v;
    
     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }

     v = r100_init_vb( rdrv, rdev, VF_PRIM_TYPE_RECTANGLE_LIST, 3, 12 );
     if (rdev->blittingflags & DSBLIT_ROTATE180) {
          *v++ = f2d(dr->x);       *v++ = f2d(dr->y);       *v++ = f2d(sr->x+sr->w); *v++ = f2d(sr->y+sr->h);
          *v++ = f2d(dr->x+dr->w); *v++ = f2d(dr->y);       *v++ = f2d(sr->x);       *v++ = f2d(sr->y+sr->h);
          *v++ = f2d(dr->x+dr->w); *v++ = f2d(dr->y+dr->h); *v++ = f2d(sr->x);       *v++ = f2d(sr->y);
     }
     else {
          *v++ = f2d(dr->x);       *v++ = f2d(dr->y);       *v++ = f2d(sr->x);       *v++ = f2d(sr->y);
          *v++ = f2d(dr->x+dr->w); *v++ = f2d(dr->y);       *v++ = f2d(sr->x+sr->w); *v++ = f2d(sr->y);
          *v++ = f2d(dr->x+dr->w); *v++ = f2d(dr->y+dr->h); *v++ = f2d(sr->x+sr->w); *v++ = f2d(sr->y+sr->h);
     }
     
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
          for (i = 0; i < 10; i++) {
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].x) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].y) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].z) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].w) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].s) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].t) );
          }
          ve += 10;
     }

     if (num > 0) {
          radeon_waitfifo( rdrv, rdev, num*6 );
          for (i = 0; i < num; i++) {
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].x) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].y) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].z) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].w) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].s) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].t) );
          }
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
     
     if (DFB_PLANAR_PIXELFORMAT(rdev->dst_format)) {
          DFBRegion   *clip = &rdev->clip;
          volatile u8 *mmio = rdrv->mmio_base;
          bool         s420 = DFB_PLANAR_PIXELFORMAT(rdev->src_format);
          int          i;
          
          /* Scale coordinates */
          for (i = 0; i < num; i++) {
               ve[i].x *= 0.5;
               ve[i].y *= 0.5;
          }

          /* Prepare Cb plane */
          radeon_waitfifo( rdrv, rdev, s420 ? 8 : 5 );
          radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
          radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
          if (s420) {
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
          radeon_waitfifo( rdrv, rdev, s420 ? 3 : 2 );
          radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
          if (s420)
               radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cr );
          radeon_out32( mmio, PP_TFACTOR_0, rdev->cr_cop );

          /* Map Cr plane */
          r100DoTextureTriangles( rdrv, rdev, ve, num, prim );
     
          /* Reset */
          radeon_waitfifo( rdrv, rdev, s420 ? 8 : 5 );
          radeon_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
          radeon_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
          if (s420) {
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
     }

     return true;
}

void r100EmitCommands3D( void *drv, void *dev )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     
     if (rdev->vb_count)
          r100_flush_vb( rdrv, rdev );
}
