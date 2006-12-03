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
out_vertex2d0( volatile u8 *mmio, float x, float y, float c[4] )
{
     radeon_out32( mmio, SE_PORT_DATA0, f2d(x) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(y) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(0) ); // z
     radeon_out32( mmio, SE_PORT_DATA0, f2d(1) ); // w
     radeon_out32( mmio, SE_PORT_DATA0, f2d(c[0]) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(c[1]) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(c[2]) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(c[3]) );
}

static __inline__ void
out_vertex2d2( volatile u8 *mmio, float x, float y, float s, float t )
{
     radeon_out32( mmio, SE_PORT_DATA0, f2d(x) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(y) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(0) ); // z
     radeon_out32( mmio, SE_PORT_DATA0, f2d(1) ); // w
     radeon_out32( mmio, SE_PORT_DATA0, f2d(s) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(t) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(0) ); // r
     radeon_out32( mmio, SE_PORT_DATA0, f2d(1) ); // q
}

static __inline__ void
out_vertex3d( volatile u8 *mmio,
              float x, float y, float z, float w, float s, float t )
{
     radeon_out32( mmio, SE_PORT_DATA0, f2d(x) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(y) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(z) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(1) ); // FIXME 
     radeon_out32( mmio, SE_PORT_DATA0, f2d(s) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(t) );
     radeon_out32( mmio, SE_PORT_DATA0, f2d(0) ); // r
     radeon_out32( mmio, SE_PORT_DATA0, f2d(1) ); // q
}


static void
r300DoFillRectangle3D( RadeonDriverData *rdrv,
                       RadeonDeviceData *rdev,
                       DFBRectangle     *rect )
{
     volatile u8 *mmio = rdrv->mmio_base;

     if (rect->w == 1 && rect->h == 1) {
          radeon_waitfifo( rdrv, rdev, 1+1*8 );
          
          radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_POINT_LIST |
                                          VF_PRIM_WALK_DATA      |
                                          (1 << VF_NUM_VERTICES_SHIFT) );

          out_vertex2d0( mmio, rect->x, rect->y, rdev->color );
     }
     else {
          radeon_waitfifo( rdrv, rdev, 1+4*8 );
     
          radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_QUAD_LIST |
                                          VF_PRIM_WALK_DATA      |
                                          (4 << VF_NUM_VERTICES_SHIFT) );

          out_vertex2d0( mmio, rect->x        , rect->y        , rdev->color );
          out_vertex2d0( mmio, rect->x+rect->w, rect->y        , rdev->color );
          out_vertex2d0( mmio, rect->x+rect->w, rect->y+rect->h, rdev->color );
          out_vertex2d0( mmio, rect->x        , rect->y+rect->h, rdev->color );
     }
}

bool r300FillRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     r300DoFillRectangle3D( rdrv, rdev, rect );

     return true;
}

bool r300FillRectangle3D_420( void *drv, void *dev, DFBRectangle *rect )
{
     return false;
}

static void
r300DoFillTriangle( RadeonDriverData *rdrv,
                    RadeonDeviceData *rdev,
                    DFBTriangle      *tri )
{
     volatile u8 *mmio = rdrv->mmio_base;

     radeon_waitfifo( rdrv, rdev, 1+3*8 );
     
     radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_TRIANGLE_LIST |
                                     VF_PRIM_WALK_DATA          |
                                     (3 << VF_NUM_VERTICES_SHIFT) );
     
     out_vertex2d0( mmio, tri->x1, tri->y1, rdev->color );
     out_vertex2d0( mmio, tri->x2, tri->y2, rdev->color );
     out_vertex2d0( mmio, tri->x3, tri->y3, rdev->color );
}

bool r300FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     r300DoFillTriangle( rdrv, rdev, tri );
     
     return true;
}

bool r300FillTriangle_420( void *drv, void *dev, DFBTriangle *tri )
{
     return false;
}

static void
r300DoDrawRectangle3D( RadeonDriverData *rdrv,
                       RadeonDeviceData *rdev,
                       DFBRectangle     *rect )
{
#if 0
     volatile u8 *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 1+16*8 );
          
     radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_QUAD_LIST |
                                     VF_PRIM_WALK_DATA      |
                                     (16 << VF_NUM_VERTICES_SHIFT) );
     /* top line */
     out_vertex2d0( mmio, rect->x          , rect->y          , rdev->color );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y          , rdev->color );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y+1        , rdev->color );
     out_vertex2d0( mmio, rect->x          , rect->y+1        , rdev->color );
     /* right line */
     out_vertex2d0( mmio, rect->x+rect->w-1, rect->y+1        , rdev->color );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y+1        , rdev->color );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y+rect->h-1, rdev->color );
     out_vertex2d0( mmio, rect->x+rect->w-1, rect->y+rect->h-1, rdev->color );
     /* bottom line */
     out_vertex2d0( mmio, rect->x          , rect->y+rect->h-1, rdev->color );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y+rect->h-1, rdev->color );
     out_vertex2d0( mmio, rect->x+rect->w  , rect->y+rect->h  , rdev->color );
     out_vertex2d0( mmio, rect->x          , rect->y+rect->h  , rdev->color );
     /* left line */
     out_vertex2d0( mmio, rect->x          , rect->y+1        , rdev->color );
     out_vertex2d0( mmio, rect->x+1        , rect->y+1        , rdev->color );
     out_vertex2d0( mmio, rect->x+1        , rect->y+rect->h-1, rdev->color );
     out_vertex2d0( mmio, rect->x          , rect->y+rect->h-1, rdev->color );
#else
     DFBRectangle tmp;
     /* top line */
     tmp = (DFBRectangle) { rect->x, rect->y, rect->w, 1 };
     r300DoFillRectangle3D( rdrv, rdev, &tmp );
     /* right line */
     tmp = (DFBRectangle) { rect->x+rect->w-1, rect->y+1, 1, rect->h-2 };
     r300DoFillRectangle3D( rdrv, rdev, &tmp );
     /* bottom line */
     tmp = (DFBRectangle) { rect->x, rect->y+rect->h-1, rect->w, 1 };
     r300DoFillRectangle3D( rdrv, rdev, &tmp );
     /* left line */
     tmp = (DFBRectangle) { rect->x, rect->y+1, 1, rect->h-2 };
     r300DoFillRectangle3D( rdrv, rdev, &tmp );
#endif
}

bool r300DrawRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     r300DoDrawRectangle3D( rdrv, rdev, rect );
     
     return true;
}

bool r300DrawRectangle3D_420( void *drv, void *dev, DFBRectangle *rect )
{
     return false;
}

static void
r300DoDrawLine3D( RadeonDriverData *rdrv,
                  RadeonDeviceData *rdev,
                  DFBRegion        *line )
{
     volatile u8 *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 1+2*8 );
     
     radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_LINE_LIST |
                                     VF_PRIM_WALK_DATA      |
                                     (2 << VF_NUM_VERTICES_SHIFT) );

     out_vertex2d0( mmio, line->x1, line->y1, rdev->color );
     out_vertex2d0( mmio, line->x2, line->y2, rdev->color );
}

bool r300DrawLine3D( void *drv, void *dev, DFBRegion *line )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;

     r300DoDrawLine3D( rdrv, rdev, line );

     return true;
}

bool r300DrawLine3D_420( void *drv, void *dev, DFBRegion *line )
{
     return false;
}

static void
r300DoBlit3D( RadeonDriverData *rdrv, RadeonDeviceData *rdev,
              DFBRectangle     *sr,   DFBRectangle     *dr )
{
     volatile u8 *mmio = rdrv->mmio_base;
     float        sx1, sy1, sx2, sy2;

     sx1 = (float)sr->x / rdev->src_width; 
     sy1 = (float)sr->y / rdev->src_height;
     sx2 = (float)(sr->x+sr->w) / rdev->src_width;
     sy2 = (float)(sr->y+sr->h) / rdev->src_height;

     radeon_waitfifo( rdrv, rdev, 1+4*8 );

     radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_QUAD_LIST |
                                     VF_PRIM_WALK_DATA      |
                                     (4 << VF_NUM_VERTICES_SHIFT) );
     
     out_vertex2d2( mmio, dr->x      , dr->y      , sx1, sy1 );
     out_vertex2d2( mmio, dr->x+dr->w, dr->y      , sx2, sy1 );
     out_vertex2d2( mmio, dr->x+dr->w, dr->y+dr->h, sx2, sy2 );
     out_vertex2d2( mmio, dr->x      , dr->y+dr->h, sx1, sy2 );
}

bool r300Blit3D( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     DFBRectangle      dr   = { dx, dy, sr->w, sr->h };

     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }

     r300DoBlit3D( rdrv, rdev, sr, &dr );
     
     return true;
}

bool r300Blit3D_420( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     DFBRectangle dr = { dx, dy, sr->w, sr->h };

     return r300StretchBlit_420( drv, dev, sr, &dr );
}

bool r300StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     
     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }
     
     r300DoBlit3D( rdrv, rdev, sr, dr );
     
     return true;
}

bool r300StretchBlit_420( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     return false;
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
          for (i = 0; i < 8; i++)
               out_vertex3d( mmio, ve[i].x, ve[i].y, ve[i].z,
                                   ve[i].w, ve[i].s, ve[i].t );
          ve += 8;
     }

     if (num > 0) {
          radeon_waitfifo( rdrv, rdev, num*8 );
          for (i = 0; i < num; i++)
               out_vertex3d( mmio, ve[i].x, ve[i].y, ve[i].z,
                                   ve[i].w, ve[i].s, ve[i].t );
     }
}

bool r300TextureTriangles( void *drv, void *dev, DFBVertex *ve,
                           int num, DFBTriangleFormation formation )
{ 
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     u32               prim = 0;

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
     
     r300DoTextureTriangles( rdrv, rdev, ve, num, prim );

     return true;
}

bool r300TextureTriangles_420( void *drv, void *dev, DFBVertex *ve,
                               int num, DFBTriangleFormation formation )
{
     return false;
}


void r300EmitCommands3D( void *drv, void *dev )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     volatile u8      *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, R300_RB3D_DSTCACHE_CTLSTAT, 0xa );
     radeon_out32( mmio, 0x4f18, 0x3 );
}
     
