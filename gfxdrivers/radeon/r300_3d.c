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
#include "radeon_state.h"
#include "radeon_3d.h"


#define EMIT_VERTICES( rdrv, rdev, mmio ) { \
     u32 *_v = (rdev)->vb; \
     u32  _s = (rdev)->vb_size; \
     radeon_waitfifo( rdrv, rdev, 1 ); \
     radeon_out32( mmio, SE_VF_CNTL, rdev->vb_type | VF_PRIM_WALK_DATA | \
                                    (rdev->vb_count << VF_NUM_VERTICES_SHIFT) ); \
     do { \
          u32 _n = MIN(_s, 64); \
          _s -= _n; \
          radeon_waitfifo( rdrv, rdev, _n ); \
          while (_n--) \
               radeon_out32( mmio, SE_PORT_DATA0, *_v++ ); \
     } while (_s); \
     radeon_waitfifo( rdrv, rdev, 2 ); \
     radeon_out32( mmio, R300_RB3D_DSTCACHE_CTLSTAT, 0xa ); \
     radeon_out32( mmio, 0x4f18, 0x3 ); \
}

static void
r300_flush_vb( RadeonDriverData *rdrv, RadeonDeviceData *rdev )
{
     volatile u8 *mmio = rdrv->mmio_base;
    
     EMIT_VERTICES( rdrv, rdev, mmio );
     
     if (DFB_PLANAR_PIXELFORMAT(rdev->dst_format)) {
          DFBRegion clip;
          int       i;
          
          for (i = 0; i < rdev->vb_size; i += 8) {
               rdev->vb[i+0] = f2d(d2f(rdev->vb[i+0])*0.5f);
               rdev->vb[i+1] = f2d(d2f(rdev->vb[i+1])*0.5f);
          }
          
          clip.x1 = rdev->clip.x1 >> 1;
          clip.y1 = rdev->clip.y1 >> 1;
          clip.x2 = rdev->clip.x2 >> 1;
          clip.y2 = rdev->clip.y2 >> 1;
     
          /* Prepare Cb plane */
          radeon_waitfifo( rdrv, rdev, 5 );
          radeon_out32( mmio, R300_RB3D_COLOROFFSET0, rdev->dst_offset_cb );
          radeon_out32( mmio, R300_RB3D_COLORPITCH0, (rdev->dst_pitch>>1) | 
                                                     R300_COLOR_FORMAT_RGB8 );
          radeon_out32( mmio, R300_TX_SIZE_0, ((rdev->src_width/2 -1) << R300_TX_WIDTH_SHIFT)  |
                                              ((rdev->src_height/2-1) << R300_TX_HEIGHT_SHIFT) |
                                              R300_TX_SIZE_TXPITCH_EN );
          radeon_out32( mmio, R300_TX_PITCH_0, (rdev->src_pitch>>1) - 8 );
          radeon_out32( mmio, R300_TX_OFFSET_0, rdev->src_offset_cb ); 
          r300_set_clip3d( rdrv, rdev, &clip );
     
          /* Fill Cb plane */
          EMIT_VERTICES( rdrv, rdev, mmio );
     
          /* Prepare Cr plane */
          radeon_waitfifo( rdrv, rdev, 2 );
          radeon_out32( mmio, R300_RB3D_COLOROFFSET0, rdev->dst_offset_cr );
          radeon_out32( mmio, R300_TX_OFFSET_0, rdev->src_offset_cr );
     
          /* Fill Cr plane */
          EMIT_VERTICES( rdrv, rdev, mmio );
     
          /* Reset */
          radeon_waitfifo( rdrv, rdev, 5 );
          radeon_out32( mmio, R300_RB3D_COLOROFFSET0, rdev->dst_offset );
          radeon_out32( mmio, R300_RB3D_COLORPITCH0, rdev->dst_pitch | 
                                                     R300_COLOR_FORMAT_RGB8 );
          radeon_out32( mmio, R300_TX_SIZE_0, ((rdev->src_width -1) << R300_TX_WIDTH_SHIFT)  |
                                              ((rdev->src_height-1) << R300_TX_HEIGHT_SHIFT) |
                                         R300_TX_SIZE_TXPITCH_EN );
          radeon_out32( mmio, R300_TX_PITCH_0, rdev->src_pitch - 8 );
          radeon_out32( mmio, R300_TX_OFFSET_0, rdev->src_offset ); 
          r300_set_clip3d( rdrv, rdev, &rdev->clip );
     }
    
     rdev->vb_size  = 0;
     rdev->vb_count = 0;
}

static inline u32*
r300_init_vb( RadeonDriverData *rdrv, RadeonDeviceData *rdev, u32 type, u32 count, u32 size )
{
     u32 *vb;
    
     if (rdev->vb_size+size > sizeof(rdev->vb)/sizeof(rdev->vb[0]))
          r300_flush_vb( rdrv, rdev );
        
     vb = &rdev->vb[rdev->vb_size];
     rdev->vb_type   = type;
     rdev->vb_size  += size;
     rdev->vb_count += count;
    
     return vb;
}


#define VTX(v, x, y, c)   \
     *(v)++ = f2d(x);     \
     *(v)++ = f2d(y);     \
     *(v)++ = f2d(0);     \
     *(v)++ = f2d(1);     \
     *(v)++ = f2d(c[0]);  \
     *(v)++ = f2d(c[1]);  \
     *(v)++ = f2d(c[2]);  \
     *(v)++ = f2d(c[3])

bool r300FillRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     float             x1, y1;
     float             x2, y2;
     u32              *v;
     
     x1 = rect->x;         y1 = rect->y;
     x2 = rect->x+rect->w; y2 = rect->y+rect->h;
     if (rdev->matrix) {
          float x, y;

          v = r300_init_vb( rdrv, rdev, VF_PRIM_TYPE_QUAD_LIST, 4, 32 );
          RADEON_TRANSFORM( x1, y1, x, y, rdev->matrix );
          VTX( v, x, y, rdev->color );
          RADEON_TRANSFORM( x2, y1, x, y, rdev->matrix );
          VTX( v, x, y, rdev->color );
          RADEON_TRANSFORM( x2, y2, x, y, rdev->matrix );
          VTX( v, x, y, rdev->color );
          RADEON_TRANSFORM( x1, y2, x, y, rdev->matrix );
          VTX( v, x, y, rdev->color );
     }
     else { 
          v = r300_init_vb( rdrv, rdev, VF_PRIM_TYPE_QUAD_LIST, 4, 32 );
          VTX( v, x1, y1, rdev->color );
          VTX( v, x2, y1, rdev->color );
          VTX( v, x2, y2, rdev->color );
          VTX( v, x1, y2, rdev->color );
     }

     return true;
}

bool r300FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     float             x1, y1;
     float             x2, y2;
     float             x3, y3;
     u32              *v;

     x1 = tri->x1; y1 = tri->y1;
     x2 = tri->x2; y2 = tri->y2;
     x3 = tri->x3; y3 = tri->y3;
     if (rdev->matrix) {
          RADEON_TRANSFORM( x1, y1, x1, y1, rdev->matrix );
          RADEON_TRANSFORM( x2, y2, x2, y2, rdev->matrix );
          RADEON_TRANSFORM( x3, y3, x3, y3, rdev->matrix );
     }

     v = r300_init_vb( rdrv, rdev, VF_PRIM_TYPE_TRIANGLE_LIST, 3, 24 );
     VTX( v, x1, y1, rdev->color );
     VTX( v, x2, y2, rdev->color );
     VTX( v, x3, y3, rdev->color );
     
     return true;
}

bool r300DrawRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     DFBRectangle tmp;
     
     /* top line */
     tmp = (DFBRectangle) { rect->x, rect->y, rect->w, 1 };
     r300FillRectangle3D( drv, dev, &tmp );
     /* right line */
     tmp = (DFBRectangle) { rect->x+rect->w-1, rect->y+1, 1, rect->h-2 };
     r300FillRectangle3D( drv, dev, &tmp );
     /* bottom line */
     tmp = (DFBRectangle) { rect->x, rect->y+rect->h-1, rect->w, 1 };
     r300FillRectangle3D( drv, dev, &tmp );
     /* left line */
     tmp = (DFBRectangle) { rect->x, rect->y+1, 1, rect->h-2 };
     r300FillRectangle3D( drv, dev, &tmp );
     
     return true;
}

bool r300DrawLine3D( void *drv, void *dev, DFBRegion *line )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     float             x1, y1;
     float             x2, y2;
     u32              *v;

     x1 = line->x1; y1 = line->y1;
     x2 = line->x2; y2 = line->y2;
     if (rdev->matrix) {
          RADEON_TRANSFORM( x1, y1, x1, y1, rdev->matrix );
          RADEON_TRANSFORM( x2, y2, x2, y2, rdev->matrix );
     }
     
     v = r300_init_vb( rdrv, rdev, VF_PRIM_TYPE_LINE_LIST, 2, 16 );
     VTX( v, x1, y1, rdev->color );
     VTX( v, x2, y2, rdev->color );

     return true;
}

#undef VTX
#define VTX( v, x, y, s, t ) \
     *(v)++ = f2d(x);        \
     *(v)++ = f2d(y);        \
     *(v)++ = f2d(0);        \
     *(v)++ = f2d(1);        \
     *(v)++ = f2d(s);        \
     *(v)++ = f2d(t);        \
     *(v)++ = f2d(0);        \
     *(v)++ = f2d(1) 

bool r300Blit3D( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     DFBRectangle dr = { dx, dy, sr->w, sr->h };

     return r300StretchBlit( drv, dev, sr, &dr );
}

bool r300StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     float             x1, y1;
     float             x2, y2;
     float             s1, s2;
     float             t1, t2;
     u32              *v;
     
     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }
   
     s1 = (float)sr->x / rdev->src_width;  s2 = (float)(sr->x+sr->w) / rdev->src_width;
     t1 = (float)sr->y / rdev->src_height; t2 = (float)(sr->y+sr->h) / rdev->src_height;
     if (rdev->blittingflags & DSBLIT_ROTATE180) {
          float tmp;
          tmp = s2; s2 = s1; s1 = tmp;
          tmp = t2; t2 = t1; t1 = tmp;
     }

     x1 = dr->x;       y1 = dr->y;
     x2 = dr->x+dr->w; y2 = dr->y+dr->h;
     if (rdev->matrix) {
          float x, y;

          v = r300_init_vb( rdrv, rdev, VF_PRIM_TYPE_QUAD_LIST, 4, 32 );
          RADEON_TRANSFORM( x1, y1, x, y, rdev->matrix );
          VTX( v, x, y, s1, t1 );
          RADEON_TRANSFORM( x2, y1, x, y, rdev->matrix );
          VTX( v, x, y, s2, t1 );
          RADEON_TRANSFORM( x2, y2, x, y, rdev->matrix );
          VTX( v, x, y, s2, t2 );
          RADEON_TRANSFORM( x1, y2, x, y, rdev->matrix );
          VTX( v, x, y, s1, t2 );
     }
     else {
          v = r300_init_vb( rdrv, rdev, VF_PRIM_TYPE_QUAD_LIST, 4, 32 );
          VTX( v, x1, y1, s1, t1 );
          VTX( v, x2, y1, s2, t1 );
          VTX( v, x2, y2, s2, t2 );
          VTX( v, x1, y2, s1, t2 );
     }
     
     return true;
}

static void
r300DoTextureTriangles( RadeonDriverData *rdrv, RadeonDeviceData *rdev,
                        DFBVertex *ve, int num, u32 primitive )
{
     volatile u8 *mmio = rdrv->mmio_base;
     int          i;
 
     radeon_waitfifo( rdrv, rdev, 1 ); 
     
     radeon_out32( mmio, SE_VF_CNTL, primitive | VF_PRIM_WALK_DATA |
                                     (num << VF_NUM_VERTICES_SHIFT) );

     for (; num >= 8; num -= 8) {
          radeon_waitfifo( rdrv, rdev, 64 );
          for (i = 0; i < 8; i++) {
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].x) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].y) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].z) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(1) ); // FIXME 
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].s) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].t) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(0) ); // r
               radeon_out32( mmio, SE_PORT_DATA0, f2d(1) ); // q
          }
          ve += 8;
     }

     if (num > 0) {
          radeon_waitfifo( rdrv, rdev, num*8 );
          for (i = 0; i < num; i++) {
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].x) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].y) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].z) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(1) ); // FIXME 
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].s) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(ve[i].t) );
               radeon_out32( mmio, SE_PORT_DATA0, f2d(0) ); // r
               radeon_out32( mmio, SE_PORT_DATA0, f2d(1) ); // q
          }
     }
     
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, R300_RB3D_DSTCACHE_CTLSTAT, 0xa );
     radeon_out32( mmio, 0x4f18, 0x3 );
}

bool r300TextureTriangles( void *drv, void *dev, DFBVertex *ve,
                           int num, DFBTriangleFormation formation )
{ 
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     u32               prim = 0;
     int               i;

     if (num > 65535) {
          D_WARN( "R300 supports maximum 65535 vertices" );
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
     
     if (rdev->matrix) {
          for (i = 0; i < num; i++)
               RADEON_TRANSFORM( ve[i].x, ve[i].y, ve[i].x, ve[i].y, rdev->matrix );
     }
     
     r300DoTextureTriangles( rdrv, rdev, ve, num, prim );
     
     if (DFB_PLANAR_PIXELFORMAT(rdev->dst_format)) {
          volatile u8 *mmio = rdrv->mmio_base;
          DFBRegion    clip;
          int          i;
          
          /* Scale coordinates */
          for (i = 0; i < num; i++) {
               ve[i].x *= 0.5;
               ve[i].y *= 0.5;
          }
          clip.x1 = rdev->clip.x1 >> 1;
          clip.y1 = rdev->clip.y1 >> 1;
          clip.x2 = rdev->clip.x2 >> 1;
          clip.y2 = rdev->clip.y2 >> 1;
          
          /* Prepare Cb plane */
          radeon_waitfifo( rdrv, rdev, 5 );
          radeon_out32( mmio, R300_RB3D_COLOROFFSET0, rdev->dst_offset_cb );
          radeon_out32( mmio, R300_RB3D_COLORPITCH0, (rdev->dst_pitch>>1) | 
                                                     R300_COLOR_FORMAT_RGB8 );
          radeon_out32( mmio, R300_TX_SIZE_0, ((rdev->src_width/2 -1) << R300_TX_WIDTH_SHIFT)  |
                                              ((rdev->src_height/2-1) << R300_TX_HEIGHT_SHIFT) |
                                              R300_TX_SIZE_TXPITCH_EN );
          radeon_out32( mmio, R300_TX_PITCH_0, (rdev->src_pitch>>1) - 8 );
          radeon_out32( mmio, R300_TX_OFFSET_0, rdev->src_offset_cb ); 
          r300_set_clip3d( rdrv, rdev, &clip );
     
          /* Blit Cb plane */
          r300DoTextureTriangles( rdrv, rdev, ve, num, prim );
     
          /* Prepare Cr plane */
          radeon_waitfifo( rdrv, rdev, 2 );
          radeon_out32( mmio, R300_RB3D_COLOROFFSET0, rdev->dst_offset_cr );
          radeon_out32( mmio, R300_TX_OFFSET_0, rdev->src_offset_cr );
     
          /* Blit Cr plane */
          r300DoTextureTriangles( rdrv, rdev, ve, num, prim );
     
          /* Reset */
          radeon_waitfifo( rdrv, rdev, 5 );
          radeon_out32( mmio, R300_RB3D_COLOROFFSET0, rdev->dst_offset );
          radeon_out32( mmio, R300_RB3D_COLORPITCH0, rdev->dst_pitch | 
                                                     R300_COLOR_FORMAT_RGB8 );
          radeon_out32( mmio, R300_TX_SIZE_0, ((rdev->src_width -1) << R300_TX_WIDTH_SHIFT)  |
                                              ((rdev->src_height-1) << R300_TX_HEIGHT_SHIFT) |
                                              R300_TX_SIZE_TXPITCH_EN );
          radeon_out32( mmio, R300_TX_PITCH_0, rdev->src_pitch - 8 );
          radeon_out32( mmio, R300_TX_OFFSET_0, rdev->src_offset ); 
          r300_set_clip3d( rdrv, rdev, &rdev->clip );
     }
     
     return true;
}

void r300EmitCommands3D( void *drv, void *dev )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     if (rdev->vb_count)
          r300_flush_vb( rdrv, rdev );
} 
