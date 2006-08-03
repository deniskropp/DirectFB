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
out_vertex2d0( volatile __u8 *mmio, float x, float y, float c[4] )
{
     *((volatile float*)(mmio+SE_PORT_DATA0)) = x;
     *((volatile float*)(mmio+SE_PORT_DATA0)) = y;
     *((volatile float*)(mmio+SE_PORT_DATA0)) = 0.0; // z
     *((volatile float*)(mmio+SE_PORT_DATA0)) = 1.0; // w
     *((volatile float*)(mmio+SE_PORT_DATA0)) = c[0];
     *((volatile float*)(mmio+SE_PORT_DATA0)) = c[1];
     *((volatile float*)(mmio+SE_PORT_DATA0)) = c[2];
     *((volatile float*)(mmio+SE_PORT_DATA0)) = c[3];
}

static __inline__ void
out_vertex2d2( volatile __u8 *mmio, float x, float y, float s, float t )
{
     *((volatile float*)(mmio+SE_PORT_DATA0)) = x;
     *((volatile float*)(mmio+SE_PORT_DATA0)) = y;
     *((volatile float*)(mmio+SE_PORT_DATA0)) = 0.0; // z
     *((volatile float*)(mmio+SE_PORT_DATA0)) = 1.0; // w
     *((volatile float*)(mmio+SE_PORT_DATA0)) = s;
     *((volatile float*)(mmio+SE_PORT_DATA0)) = t;
     *((volatile float*)(mmio+SE_PORT_DATA0)) = 0.0; // r
     *((volatile float*)(mmio+SE_PORT_DATA0)) = 1.0; // q
}


static void
r300DoFillRectangle3D( RadeonDriverData *rdrv,
                       RadeonDeviceData *rdev,
                       DFBRectangle     *rect )
{
     volatile __u8 *mmio = rdrv->mmio_base;

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
     volatile __u8 *mmio = rdrv->mmio_base;

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
     volatile __u8 *mmio = rdrv->mmio_base;
     
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
     volatile __u8 *mmio = rdrv->mmio_base;
     
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
     volatile __u8 *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 1+4*8 );

     radeon_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_QUAD_LIST |
                                     VF_PRIM_WALK_DATA      |
                                     (4 << VF_NUM_VERTICES_SHIFT) );
     
     out_vertex2d2( mmio, dr->x      , dr->y      , sr->x      , sr->y       );
     out_vertex2d2( mmio, dr->x+dr->w, dr->y      , sr->x+sr->w, sr->y       );
     out_vertex2d2( mmio, dr->x+dr->w, dr->y+dr->h, sr->x+sr->w, sr->y+sr->h );
     out_vertex2d2( mmio, dr->x      , dr->y+dr->h, sr->x      , sr->y+sr->h );
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


void r300EmitCommands3D( void *drv, void *dev )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     volatile __u8    *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, R300_RB3D_DSTCACHE_CTLSTAT, 0xa );
     radeon_out32( mmio, 0x4f18, 0x3 );
}
     
