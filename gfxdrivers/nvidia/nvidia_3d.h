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

#ifndef __NVIDIA_3D_H__
#define __NVIDIA_3D_H__


bool nvFillRectangle3D( void *drv, void *dev, DFBRectangle *rect );

bool nvFillTriangle3D( void *drv, void *dev, DFBTriangle *tri );

bool nvDrawRectangle3D( void *drv, void *dev, DFBRectangle *rect );

bool nvDrawLine3D( void *drv, void *dev, DFBRegion *line );

bool nvTextureTriangles( void *drv, void *dev, DFBVertex *vertices,
                         int num, DFBTriangleFormation formation );


#define VINC  0xAAAAAAAC
#define VMASK 0x55555555
#define UINC  0x55555558
#define UMASK 0xAAAAAAAA

static inline void
argb1555_to_tex( __u32 *dst, __u8 *src, int pitch, int width, int height )
{
     int u, v, i;

     for (v = 0; height--; v = (v + VINC) & VMASK) {
          for (i = 0, u = 0; i < width/2; i++, u = (u + UINC) & UMASK) {
               register __u32 pix0, pix1;
               pix0 = ((__u32*) src)[i];
               pix1 = pix0 >> 16;
               pix0 = ARGB1555_TO_RGB16( pix0 );
               pix1 = ARGB1555_TO_RGB16( pix1 );
               dst[(u|v)/4] = pix0 | (pix1 << 16);
          }
          
          if (width & 1) {
               u = (u + UINC) & UMASK;
               dst[(u|v)/4] = ARGB1555_TO_RGB16( ((__u16*) src)[width-1] );
          }             
               
          src += pitch;
     }
}

static inline void
rgb16_to_tex( __u32 *dst, __u8 *src, int pitch, int width, int height )
{
     int u, v, i;

     for (v = 0; height--; v = (v + VINC) & VMASK) {
          for (i = 0, u = 0; i < width/2; i++, u = (u + UINC) & UMASK)
               dst[(u|v)/4] = ((__u32*) src)[i];
          
          if (width & 1) {
               u = (u + UINC) & UMASK;
               dst[(u|v)/4] = ((__u16*) src)[width-1];
          }             
               
          src += pitch;
     }
}

static inline void
rgb32_to_tex( __u32 *dst, __u8 *src, int pitch, int width, int height )
{
     int u, v, i;

     for (v = 0; height--; v = (v + VINC) & VMASK) {
          for (i = 0, u = 0; i < width; i += 2, u = (u + UINC) & UMASK) {
               register __u32 pix0, pix1;
               pix0 = ((__u32*) src)[i];
               pix0 = RGB32_TO_RGB16( pix0 );
               pix1 = ((__u32*) src)[i+1];
               pix1 = RGB32_TO_RGB16( pix1 );
               dst[(u|v)/4] = pix0 | (pix1 << 16);
          }
          
          if (width & 1) {
               u = (u + UINC) & UMASK;
               dst[(u|v)/4] = RGB32_TO_RGB16( ((__u32*) src)[width-1] );
          }             
               
          src += pitch;
     }
}

#undef VINC
#undef VMASK
#undef UINC
#undef UMASK


#endif /* __NVIDIA_3D_H__ */

