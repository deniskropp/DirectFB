/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>
              Andreas Hundt <andi@convergence.de>
              Sven Neumann <sven@convergence.de>

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

#ifndef __GENERIC_H__
#define __GENERIC_H__

#include <config.h>
/* this order is required for Intel with MMX, how about bigendian? */

typedef struct
{
     __u16 b;
     __u16 g;
     __u16 r;
     __u16 a;
} Accumulator;

#define ACC_WIDTH 1600

typedef void (*GFunc)();


int  gAquire  ( CardState *state, DFBAccelerationMask accel );
void gRelease ( CardState *state );

void gUpload  ( int offset, void *data, int len );

void gFillRectangle ( DFBRectangle *rect );
void gDrawLine      ( DFBRegion    *line );
void gFillTriangle  ( DFBTriangle  *tri );

void gBlit          ( DFBRectangle *rect, int dx, int dy );
void gStretchBlit   ( DFBRectangle *srect, DFBRectangle *drect );

#ifdef USE_MMX
void gInit_MMX();
#endif



#define SET_ALPHA_PIXEL_RGB15(d,r,g,b,a) \
     if (a) {\
          if ((a) == 0xff) {\
               *(d) = (((r)&0xf8) << 7) | (((g)&0xf8) << 2) | (((b)&0xf8) >> 3);\
          }\
          else {\
               __u32 pixel = *(d);\
               __u8  s = ((a)>>3)+1;\
               __u8 s1 = 32-s;\
               \
     	       pixel = (((((pixel & 0x00007c1f) * s1) + (((((r)&0xf8)<<7) | ((b)>>3)) * s)) & 0x000f83e0) + \
     	                ((((pixel & 0x000003e0) * s1) +   (((g)<<2)                   * s)) & 0x00007c00)) >> 5;\
               *(d) = pixel;\
          }\
     }

#define SET_ALPHA_PIXEL_RGB16(d,r,g,b,a) \
     if (a) {\
          if ((a) == 0xff) {\
               *(d) = (((r)&0xf8) << 8) | (((g)&0xfc) << 3) | (((b)&0xf8) >> 3);\
          }\
          else {\
               __u32 pixel = *(d);\
               __u8  s = ((a)>>2)+1;\
     	       __u8 s1 = 64-s;\
               \
     	       pixel = (((((pixel & 0x0000f81f) * s1) + (((((r)&0xf8)<<8) | ((b)>>3)) * s)) & 0x003e07c0) + \
     	                ((((pixel & 0x000007e0) * s1) +   (((g)<<3)                   * s)) & 0x0001f800)) >> 6;\
               *(d) = pixel;\
          }\
     }

#define SET_ALPHA_PIXEL_RGB24(d,r,g,b,a)\
     if (a) {\
          __u8 *pixel = (d);\
          \
          if ((a) == 0xff) {\
               *pixel++ = (b);\
               *pixel++ = (g);\
               *pixel++ = (r);\
          }\
          else {\
               __u16  s = (a)+1;\
               __u16 s1 = 256-s;\
               \
               *pixel++ = (((*pixel) * s1) + ((b) * s)) >> 8;\
               *pixel++ = (((*pixel) * s1) + ((g) * s)) >> 8;\
               *pixel++ = (((*pixel) * s1) + ((r) * s)) >> 8;\
          }\
     }

#define SET_ALPHA_PIXEL_RGB32(d,r,g,b,a)\
     if (a) {\
          if ((a) == 0xff) {\
               *(d) = (0xff000000 | ((r)<<16) | ((g)<<8) | (b));\
          }\
          else {\
               __u32 pixel = *(d);\
               __u16  s = (a)+1;\
     	       __u16 s1 = 256-s;\
               \
     	       *(d) = (((((pixel & 0x00ff00ff) * s1) + ((((r)<<16) | (b)) * s)) & 0xff00ff00) + \
     	               ((((pixel & 0x0000ff00) * s1) +  (((g)<<8)         * s)) & 0x00ff0000)) >> 8;\
          }\
     }

#define SET_ALPHA_PIXEL_ARGB(d,r,g,b,a)\
     if (a) {\
          if ((a) == 0xff) {\
               *(d) = (0xff000000 | ((r)<<16) | ((g)<<8) | (b));\
          }\
          else {\
               __u32 pixel = *(d);\
               __u16  s = (a)+1;\
     	       __u16 s1 = 256-s;\
               \
     	       *(d) = (((((pixel & 0x00ff00ff)       * s1) + ((((r)<<16) | (b)) * s)) & 0xff00ff00) >> 8) + \
                      (((((pixel & 0xff00ff00) >> 8) * s1) + ((((a)<<16) | (g)) * s)) & 0xff00ff00);\
          }\
     }

#endif
