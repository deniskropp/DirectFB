/*
   Copyright (C) 2004-2006 Claudio Ciccani <klan@users.sf.net>

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

#include <direct/messages.h>
#include <direct/mem.h>
#include <direct/memcpy.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/surface.h>

#include <gfx/convert.h>

#include "nvidia.h"
#include "nvidia_regs.h"
#include "nvidia_accel.h"
#include "nvidia_3d.h"


static __inline__ u32
f2d( float f ) {
     union {
          float f;
          u32 d;
     } t;
     t.f = f;
     return t.d;
}

#define nv_setstate3d( state3d ) {                              \
     if ((state3d)->modified) {                                 \
          nv_begin( SUBC_TEXTRIANGLE, TXTRI_COLOR_KEY, 7 );     \
          nv_outr( (state3d)->colorkey );                       \
          nv_outr( (state3d)->offset   );                       \
          nv_outr( (state3d)->format   );                       \
          nv_outr( (state3d)->filter   );                       \
          nv_outr( (state3d)->blend    );                       \
          nv_outr( (state3d)->control  );                       \
          nv_outr( (state3d)->fog      );                       \
                                                                \
          (state3d)->modified = false;                          \
     }                                                          \
}

#define nv_putvertex( i, x, y, z, w, col, spc, s, t ) {         \
     nv_begin( SUBC_TEXTRIANGLE, TXTRI_VERTEX0+(i)*32, 8 );     \
     nv_outr( f2d( x ) );                                       \
     nv_outr( f2d( y ) );                                       \
     nv_outr( f2d( z ) );                                       \
     nv_outr( f2d( w ) );                                       \
     nv_outr( col      );                                       \
     nv_outr( spc      );                                       \
     nv_outr( f2d( s ) );                                       \
     nv_outr( f2d( t ) );                                       \
}

#define nv_emit_vertices( i, v0, v1, v2, v3, v4, v5, v6, v7 ) { \
     nv_begin( SUBC_TEXTRIANGLE, TXTRI_PRIMITIVE0+(i)*4, 1 );   \
     nv_outr( ((v7) << 28) | ((v6) << 24) |                     \
              ((v5) << 20) | ((v4) << 16) |                     \
              ((v3) << 12) | ((v2) <<  8) |                     \
              ((v1) <<  4) |  (v0)          );                  \
}


static void nv_load_texture( NVidiaDriverData *nvdrv,
                             NVidiaDeviceData *nvdev );


bool nvFillRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;

     nv_setstate3d( &nvdev->state3d[0] );

     nv_putvertex( 0, rect->x,         rect->y, 
                      0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 1, rect->x+rect->w, rect->y,
                      0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 2, rect->x+rect->w, rect->y+rect->h,
                      0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 3, rect->x,         rect->y+rect->h,
                      0, 1, nvdev->color3d, 0, 0, 0 );

     nv_emit_vertices( 0, 0, 1, 2, 0, 2, 3, 0, 0 );

     return true;
}

bool nvFillTriangle3D( void *drv, void *dev, DFBTriangle *tri )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;

     nv_setstate3d( &nvdev->state3d[0] );

     nv_putvertex( 0, tri->x1, tri->y1, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 1, tri->x2, tri->y2, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 2, tri->x3, tri->y3, 0, 1, nvdev->color3d, 0, 0, 0 );

     nv_emit_vertices( 0, 0, 1, 2, 0, 0, 0, 0, 0 );

     return true;
}

bool nvDrawRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     DFBRegion         r[4];
     int               i;
    
     /* top */
     r[0].x1 = rect->x;
     r[0].y1 = rect->y;
     r[0].x2 = rect->x + rect->w;
     r[0].y2 = rect->y + 1;

     /* bottom */
     r[1].x1 = rect->x;
     r[1].y1 = rect->y + rect->h - 1;
     r[1].x2 = rect->x + rect->w;
     r[1].y2 = rect->y + rect->h;

     /* left */
     r[2].x1 = rect->x;
     r[2].y1 = rect->y + 1;
     r[2].x2 = rect->x + 1;
     r[2].y2 = rect->y + rect->h - 2;

     /* right */
     r[3].x1 = rect->x + rect->w - 1;
     r[3].y1 = rect->y + 1;
     r[3].x2 = rect->x + rect->w;
     r[3].y2 = rect->y + rect->h - 2;
     
     nv_setstate3d( &nvdev->state3d[0] );

     for (i = 0; i < 4; i++) {
          nv_putvertex( 0, r[i].x1, r[i].y1, 0, 1, nvdev->color3d, 0, 0, 0 );
          nv_putvertex( 1, r[i].x2, r[i].y1, 0, 1, nvdev->color3d, 0, 0, 0 );
          nv_putvertex( 2, r[i].x2, r[i].y2, 0, 1, nvdev->color3d, 0, 0, 0 );
          nv_putvertex( 3, r[i].x1, r[i].y2, 0, 1, nvdev->color3d, 0, 0, 0 );

          nv_emit_vertices( 0, 0, 1, 2, 0, 2, 3, 0, 0 );
     }

     return true;
}

bool nvDrawLine3D( void *drv, void *dev, DFBRegion *line )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     float             x1    = line->x1;
     float             y1    = line->y1;
     float             x2    = line->x2;
     float             y2    = line->y2;
     float             xinc  = 0.0;
     float             yinc  = 0.0;
     int               dx, dy;

     dx = abs( line->x2 - line->x1 );
     dy = abs( line->y2 - line->y1 );

     if (dx > dy) { /* more horizontal */
          xinc = 0.0;
          yinc = 0.5;
     } else {       /* more vertical */
          xinc = 0.5;
          yinc = 0.0;
     }

     nv_setstate3d( &nvdev->state3d[0] );

     nv_putvertex( 0, x1 - xinc, y1 - yinc, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 1, x1 + xinc, y1 + yinc, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 2, x2 + xinc, y2 + yinc, 0, 1, nvdev->color3d, 0, 0, 0 );
     nv_putvertex( 3, x2 - xinc, y2 - yinc, 0, 1, nvdev->color3d, 0, 0, 0 );

     nv_emit_vertices( 0, 2, 0, 1, 3, 0, 2, 0, 0 );

     return true;
}

bool nvTextureTriangles( void *drv, void *dev, DFBVertex *ve,
                         int num, DFBTriangleFormation formation )
{
     NVidiaDriverData *nvdrv   = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev   = (NVidiaDeviceData*) dev;
     float             s_scale;
     float             t_scale;
     int               i;

     /* load source texture into texture buffer */
     nv_load_texture( nvdrv, nvdev );
     
     s_scale = (float)nvdev->src_width  /
               (float)(1 << ((nvdev->state3d[1].format >> 16) & 0xF));
     t_scale = (float)nvdev->src_height /
               (float)(1 << ((nvdev->state3d[1].format >> 20) & 0xF));

     for (i = 0; i < num; i++) {
          ve[i].x += 0.5;
          ve[i].y += 0.5;
          ve[i].s *= s_scale;
          ve[i].t *= t_scale;
     }

     nv_setstate3d( &nvdev->state3d[1] );

     switch (formation) {
          case DTTF_LIST:
               for (i = 0; i < num; i += 3) {
                    nv_putvertex( 0, ve[i].x, ve[i].y, ve[i].z, ve[i].w,
                                  nvdev->color3d, 0, ve[i].s, ve[i].t );
                    nv_putvertex( 1, ve[i+1].x, ve[i+1].y, ve[i+1].z, ve[i+1].w,
                                  nvdev->color3d, 0, ve[i+1].s, ve[i+1].t );
                    nv_putvertex( 2, ve[i+2].x, ve[i+2].y, ve[i+2].z, ve[i+2].w,
                                  nvdev->color3d, 0, ve[i+2].s, ve[i+2].t );
                    nv_emit_vertices( 0, 0, 1, 2, 0, 0, 0, 0, 0 );
               }
               break;

          case DTTF_STRIP:
               nv_putvertex( 0, ve[0].x, ve[0].y, ve[0].z, ve[0].w,
                             nvdev->color3d, 0, ve[0].s, ve[0].t );
               nv_putvertex( 1, ve[1].x, ve[1].y, ve[1].z, ve[1].w,
                             nvdev->color3d, 0, ve[1].s, ve[1].t );
               nv_putvertex( 2, ve[2].x, ve[2].y, ve[2].z, ve[2].w,
                             nvdev->color3d, 0, ve[2].s, ve[2].t );
               nv_emit_vertices( 0, 0, 1, 2, 0, 0, 0, 0, 0 );
               
               for (i = 0; i < num; i++) {
                    nv_putvertex( 0, ve[i-2].x, ve[i-2].y, ve[i-2].z, ve[i-2].w,
                                  nvdev->color3d, 0, ve[i-2].s, ve[i-2].t );
                    nv_putvertex( 1, ve[i-1].x, ve[i-1].y, ve[i-1].z, ve[i-1].w,
                                  nvdev->color3d, 0, ve[i-1].s, ve[i-1].t );
                    nv_putvertex( 2, ve[i].x, ve[i].y, ve[i].z, ve[i].w,
                                  nvdev->color3d, 0, ve[i].s, ve[i].t );
                    nv_emit_vertices( 0, 0, 1, 2, 0, 0, 0, 0, 0 );
               }
               break;

          case DTTF_FAN:
               nv_putvertex( 0, ve[0].x, ve[0].y, ve[0].z, ve[0].w,
                             nvdev->color3d, 0, ve[0].s, ve[0].t );
               nv_putvertex( 1, ve[1].x, ve[1].y, ve[1].z, ve[1].w,
                             nvdev->color3d, 0, ve[1].s, ve[1].t );
               nv_putvertex( 2, ve[2].x, ve[2].y, ve[2].z, ve[2].w,
                             nvdev->color3d, 0, ve[2].s, ve[2].t );
               nv_emit_vertices( 0, 0, 1, 2, 0, 0, 0, 0, 0 );

               for (i = 0; i < num; i++) {
                    nv_putvertex( 0, ve[0].x, ve[0].y, ve[0].z, ve[0].w,
                                  nvdev->color3d, 0, ve[0].s, ve[0].t );
                    nv_putvertex( 1, ve[i-1].x, ve[i-1].y, ve[i-1].z, ve[i-1].w,
                                  nvdev->color3d, 0, ve[i-1].s, ve[i-1].t );
                    nv_putvertex( 2, ve[i].x, ve[i].y, ve[i].z, ve[i].w,
                                  nvdev->color3d, 0, ve[i].s, ve[i].t );
                    nv_emit_vertices( 0, 0, 1, 2, 0, 0, 0, 0, 0 );
               }
               break;

          default:
               D_BUG( "unexpected triangle formation" );
               return false;
     }

     return true;
}

/*
 * Surface to Texture conversion routines.
 */

#define VINC  0xAAAAAAAC
#define VMASK 0x55555555
#define UINC  0x55555558
#define UMASK 0xAAAAAAAA

static inline void
a8_to_tex( u32 *dst, u8 *src, int pitch, int width, int height )
{
     u32 u, v;
     int   i;

     for (v = 0; height--; v = (v + VINC) & VMASK) {
          for (i = 0, u = 0; i < width; i += 2, u = (u + UINC) & UMASK) {
#ifdef WORDS_BIGENDIAN
               dst[(u|v)/4] = ((src[i+0] & 0xF0) << 24) |
                              ((src[i+1] & 0xF0) <<  8) |
                              0x0FFF0FFF;
#else
               dst[(u|v)/4] = ((src[i+0] & 0xF0) <<  8) |
                              ((src[i+1] & 0xF0) << 24) |
                              0x0FFF0FFF;
#endif
          }
          
          if (width & 1) {
               u = (u + UINC) & UMASK;
               dst[(u|v)/4] = ((src[width-1] & 0xF0) << 8) | 0x0FFF;
          }             
               
          src += pitch;
     }
}

static inline void
rgb16_to_tex( u32 *dst, u8 *src, int pitch, int width, int height )
{
     u32 u, v;
     int   i;

     for (v = 0; height--; v = (v + VINC) & VMASK) {
          for (i = 0, u = 0; i < width/2; i++, u = (u + UINC) & UMASK)
               dst[(u|v)/4] = ((u32*) src)[i];
          
          if (width & 1) {
               u = (u + UINC) & UMASK;
               dst[(u|v)/4] = ((u16*) src)[width-1];
          }             
               
          src += pitch;
     }
}

static inline void
rgb32_to_tex( u32 *dst, u8 *src, int pitch, int width, int height )
{
     u32 u, v;
     int   i;

     for (v = 0; height--; v = (v + VINC) & VMASK) {
          for (i = 0, u = 0; i < width; i += 2, u = (u + UINC) & UMASK) {
               register u32 pix0, pix1;
               pix0 = ((u32*) src)[i];
               pix0 = RGB32_TO_RGB16( pix0 );
               pix1 = ((u32*) src)[i+1];
               pix1 = RGB32_TO_RGB16( pix1 );
#ifdef WORDS_BIGENDIAN
               dst[(u|v)/4] = (pix0 << 16) | pix1;
#else
               dst[(u|v)/4] = pix0 | (pix1 << 16);
#endif
          }
          
          if (width & 1) {
               u = (u + UINC) & UMASK;
               dst[(u|v)/4] = RGB32_TO_RGB16( ((u32*) src)[width-1] );
          }             
               
          src += pitch;
     }
}

static inline void
argb_to_tex( u32 *dst, u8 *src, int pitch, int width, int height )
{
     u32 u, v;
     int   i;

     for (v = 0; height--; v = (v + VINC) & VMASK) {
          for (i = 0, u = 0; i < width; i += 2, u = (u + UINC) & UMASK) {
               register u32 pix0, pix1;
               pix0 = ((u32*) src)[i];
               pix0 = ARGB_TO_ARGB4444( pix0 );
               pix1 = ((u32*) src)[i+1];
               pix1 = ARGB_TO_ARGB4444( pix1 );
#ifdef WORDS_BIGENDIAN
               dst[(u|v)/4] = (pix0 << 16) | pix1;
#else
               dst[(u|v)/4] = pix0 | (pix1 << 16);
#endif
          }

          if (width & 1) {
               u = (u + UINC) & UMASK;
               dst[(u|v)/4] = ARGB_TO_ARGB4444( ((u32*) src)[width-1] );
          }

          src += pitch;
     }
}

static void nv_load_texture( NVidiaDriverData *nvdrv,
                             NVidiaDeviceData *nvdev )
{
     CoreSurfaceBuffer *buffer = nvdev->src_texture;
     u32               *dst;
     
     dst = dfb_gfxcard_memory_virtual( nvdrv->device, nvdev->buf_offset[1] );
     
#if 0
     if (nvdev->src_interlaced) {
          if (surface->caps & DSCAPS_SEPARATED) {
               if (surface->field)
                    field_offset = nvdev->src_height * src_pitch;
          } else {
               if (surface->field)
                    field_offset = src_pitch;
               src_pitch *= 2;
          }
     }
#endif

     switch (buffer->format) {
          case DSPF_A8:
               a8_to_tex( dst, nvdev->src_lock->addr, nvdev->src_lock->pitch,
                          nvdev->src_width, nvdev->src_height );
               break;
          case DSPF_ARGB1555:
          case DSPF_RGB16:
               rgb16_to_tex( dst, nvdev->src_lock->addr, nvdev->src_lock->pitch,
                             nvdev->src_width, nvdev->src_height );
               break;
          case DSPF_RGB32:
               rgb32_to_tex( dst, nvdev->src_lock->addr, nvdev->src_lock->pitch,
                             nvdev->src_width, nvdev->src_height );
               break;
          case DSPF_ARGB:
               argb_to_tex( dst, nvdev->src_lock->addr, nvdev->src_lock->pitch,
                            nvdev->src_width, nvdev->src_height );
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               break;
     }
}

