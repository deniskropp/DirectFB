/*
   Copyright (C) 2004 Claudio Ciccani <klan82@cheapnet.it>

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

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include <direct/util.h>

#include "nvidia.h"
#include "nvidia_mmio.h"
#include "nvidia_3d.h"


#define nv_setstate3d( state3d )                                  \
{                                                                 \
     nv_waitfifo( nvdev, TexTri, 7 );                             \
     TexTri->ColorKey      = (state3d)->colorkey;                 \
     TexTri->TextureOffset = (state3d)->offset;                   \
     TexTri->TextureFormat = (state3d)->format;                   \
     TexTri->TextureFilter = (state3d)->filter;                   \
     TexTri->Blend         = (state3d)->blend;                    \
     TexTri->Control       = (state3d)->control;                  \
     TexTri->FogColor      = (state3d)->fog;                      \
}

#define nv_putvertex( ii, xx, yy, zz, ww, col, spc, s, t )        \
{                                                                 \
     nv_waitfifo( nvdev, TexTri, 8 );                             \
     TexTri->Tlvertex[(ii)].sx       = (float) (xx);              \
     TexTri->Tlvertex[(ii)].sy       = (float) (yy);              \
     TexTri->Tlvertex[(ii)].sz       = (float) (zz);              \
     TexTri->Tlvertex[(ii)].rhw      = (float) (ww);              \
     TexTri->Tlvertex[(ii)].color    = (__u32) (col);             \
     TexTri->Tlvertex[(ii)].specular = (__u32) (spc);             \
     TexTri->Tlvertex[(ii)].ts       = (float) (s);               \
     TexTri->Tlvertex[(ii)].tt       = (float) (t);               \
}

#define nv_flushvb( ii, v0, v1, v2, v3, v4, v5, v6, v7 )          \
{                                                                 \
     nv_waitfifo( nvdev, TexTri, 1 );                             \
     TexTri->DrawPrimitives[(ii)] = ((v7) << 28) | ((v6) << 24) | \
                                    ((v5) << 20) | ((v4) << 16) | \
                                    ((v3) << 12) | ((v2) <<  8) | \
                                    ((v1) <<  4) |  (v0);         \
}


bool nvFillRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData              *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData              *nvdev  = (NVidiaDeviceData*) dev;
     volatile NVTexturedTriangle05 *TexTri = nvdrv->TexTri;
     DFBRegion                      reg;

     reg.x1 = rect->x;
     reg.y1 = rect->y;
     reg.x2 = rect->x + rect->w;
     reg.y2 = rect->y + rect->h;

     nv_setstate3d( &nvdev->state3d );

     nv_putvertex( 0, reg.x1, reg.y1, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 1, reg.x2, reg.y1, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 2, reg.x2, reg.y2, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 3, reg.x1, reg.y2, 0, 1, nvdev->color3d, 0, 0, 0 );

     nv_flushvb( 0, 0, 1, 2, 0, 2, 3, 0, 0 );

     return true;
}

bool nvFillTriangle3D( void *drv, void *dev, DFBTriangle *tri )
{
     NVidiaDriverData              *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData              *nvdev  = (NVidiaDeviceData*) dev;
     volatile NVTexturedTriangle05 *TexTri = nvdrv->TexTri;

     nv_setstate3d( &nvdev->state3d );

     nv_putvertex( 0, tri->x1, tri->y1, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 1, tri->x2, tri->y2, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 2, tri->x3, tri->y3, 0, 1, nvdev->color3d, 0, 0, 0 );

     nv_flushvb( 0, 0, 1, 2, 0, 0, 0, 0, 0 );

     return true;
}

bool nvDrawRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData              *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData              *nvdev  = (NVidiaDeviceData*) dev;
     volatile NVTexturedTriangle05 *TexTri = nvdrv->TexTri;
     DFBRegion                      reg[4];
     int                            i;
    
     /* top */
     reg[0].x1 = rect->x;
     reg[0].y1 = rect->y;
     reg[0].x2 = rect->x + rect->w;
     reg[0].y2 = rect->y + 1;

     /* bottom */
     reg[1].x1 = rect->x;
     reg[1].y1 = rect->y + rect->h - 1;
     reg[1].x2 = rect->x + rect->w;
     reg[1].y2 = rect->y + rect->h;

     /* left */
     reg[2].x1 = rect->x;
     reg[2].y1 = rect->y + 1;
     reg[2].x2 = rect->x + 1;
     reg[2].y2 = rect->y + rect->h - 2;

     /* right */
     reg[3].x1 = rect->x + rect->w - 1;
     reg[3].y1 = rect->y + 1;
     reg[3].x2 = rect->x + rect->w;
     reg[3].y2 = rect->y + rect->h - 2;
     
     nv_setstate3d( &nvdev->state3d )

     for (i = 0; i < 4; i++) {
          nv_putvertex( 0, reg[i].x1, reg[i].y1, 0, 1, nvdev->color3d, 0, 0, 0 );
          nv_putvertex( 1, reg[i].x2, reg[i].y1, 0, 1, nvdev->color3d, 0, 0, 0 );
          nv_putvertex( 2, reg[i].x2, reg[i].y2, 0, 1, nvdev->color3d, 0, 0, 0 );
          nv_putvertex( 3, reg[i].x1, reg[i].y2, 0, 1, nvdev->color3d, 0, 0, 0 );

          nv_flushvb( 0, 0, 1, 2, 0, 2, 3, 0, 0 );
     }

     return true;
}

bool nvDrawLine3D( void *drv, void *dev, DFBRegion *line )
{
     NVidiaDriverData              *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData              *nvdev  = (NVidiaDeviceData*) dev;
     volatile NVTexturedTriangle05 *TexTri = nvdrv->TexTri;
     float                          x1     = line->x1;
     float                          y1     = line->y1;
     float                          x2     = line->x2;
     float                          y2     = line->y2;
     float                          xinc   = 0.0;
     float                          yinc   = 0.0;
     int                            dx, dy;

     dx = abs( line->x2 - line->x1 );
     dy = abs( line->y2 - line->y1 );

     if (dx > dy) { /* more horizontal */
          xinc = 0.0;
          yinc = 0.5;
     } else {       /* more vertical */
          xinc = 0.5;
          yinc = 0.0;
     }

     nv_setstate3d( &nvdev->state3d );

     nv_putvertex( 0, x1 - xinc, y1 - yinc, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 1, x1 + xinc, y1 + yinc, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 2, x2 + xinc, y2 + yinc, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 3, x2 - xinc, y2 - yinc, 0, 1, nvdev->color3d, 0, 0, 0 );

     nv_flushvb( 0, 2, 0, 1, 3, 0, 2, 0, 0 );

     return true;
}

bool nvTextureTriangles( void *drv, void *dev, DFBVertex *ve,
                         int num, DFBTriangleFormation formation )
{
     NVidiaDriverData              *nvdrv   = (NVidiaDriverData*) drv;
     NVidiaDeviceData              *nvdev   = (NVidiaDeviceData*) dev;
     volatile NVTexturedTriangle05 *TexTri  = nvdrv->TexTri;
     float                          s_scale;
     float                          t_scale;
     int                            i;

     s_scale = (float) nvdev->src_width  / 512.0;
     t_scale = (float) nvdev->src_height / 512.0;

     for (i = 0; i < num; i++) {
          ve[i].x += 0.5f;
          ve[i].y += 0.5f;
          ve[i].s *= s_scale;
          ve[i].t *= t_scale;
     }

     nv_setstate3d( &nvdev->state3d );

     switch (formation) {
          case DTTF_LIST:
               for (i = 0; i < num; i += 3) {
                    nv_putvertex( 0, ve[i].x, ve[i].y, ve[i].z, ve[i].w,
                                  nvdev->color3d, 0, ve[i].s, ve[i].t );
                    nv_putvertex( 1, ve[i+1].x, ve[i+1].y, ve[i+1].z, ve[i+1].w,
                                  nvdev->color3d, 0, ve[i+1].s, ve[i+1].t );
                    nv_putvertex( 2, ve[i+2].x, ve[i+2].y, ve[i+2].z, ve[i+2].w,
                                  nvdev->color3d, 0, ve[i+2].s, ve[i+2].t );
                    nv_flushvb( 0, 0, 1, 2, 0, 0, 0, 0, 0 );
               }
               break;

          case DTTF_STRIP:
               nv_putvertex( 0, ve[0].x, ve[0].y, ve[0].z, ve[0].w,
                             nvdev->color3d, 0, ve[0].s, ve[0].t );
               nv_putvertex( 1, ve[1].x, ve[1].y, ve[1].z, ve[1].w,
                             nvdev->color3d, 0, ve[1].s, ve[1].t );
               nv_putvertex( 2, ve[2].x, ve[2].y, ve[2].z, ve[2].w,
                             nvdev->color3d, 0, ve[2].s, ve[2].t );
               nv_flushvb( 0, 0, 1, 2, 0, 0, 0, 0, 0 );
               
               for (i = 0; i < num; i++) {
                    nv_putvertex( 0, ve[i-2].x, ve[i-2].y, ve[i-2].z, ve[i-2].w,
                                  nvdev->color3d, 0, ve[i-2].s, ve[i-2].t );
                    nv_putvertex( 1, ve[i-1].x, ve[i-1].y, ve[i-1].z, ve[i-1].w,
                                  nvdev->color3d, 0, ve[i-1].s, ve[i-1].t );
                    nv_putvertex( 2, ve[i].x, ve[i].y, ve[i].z, ve[i].w,
                                  nvdev->color3d, 0, ve[i].s, ve[i].t );
                    nv_flushvb( 0, 0, 1, 2, 0, 0, 0, 0, 0 );
               }
               break;

          case DTTF_FAN:
               nv_putvertex( 0, ve[0].x, ve[0].y, ve[0].z, ve[0].w,
                             nvdev->color3d, 0, ve[0].s, ve[0].t );
               nv_putvertex( 1, ve[1].x, ve[1].y, ve[1].z, ve[1].w,
                             nvdev->color3d, 0, ve[1].s, ve[1].t );
               nv_putvertex( 2, ve[2].x, ve[2].y, ve[2].z, ve[2].w,
                             nvdev->color3d, 0, ve[2].s, ve[2].t );
               nv_flushvb( 0, 0, 1, 2, 0, 0, 0, 0, 0 );

               for (i = 0; i < num; i++) {
                    nv_putvertex( 0, ve[0].x, ve[0].y, ve[0].z, ve[0].w,
                                  nvdev->color3d, 0, ve[0].s, ve[0].t );
                    nv_putvertex( 1, ve[i-1].x, ve[i-1].y, ve[i-1].z, ve[i-1].w,
                                  nvdev->color3d, 0, ve[i-1].s, ve[i-1].t );
                    nv_putvertex( 2, ve[i].x, ve[i].y, ve[i].z, ve[i].w,
                                  nvdev->color3d, 0, ve[i].s, ve[i].t );
                    nv_flushvb( 0, 0, 1, 2, 0, 0, 0, 0, 0 );
               }
               break;

          default:
               D_BUG( "unexpected triangle formation" );
               return false;
     }

     return true;
}
 
