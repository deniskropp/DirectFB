/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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


void gUpload ( int offset, void *data, int len );

void gFillRectangle ( DFBRectangle *rect );
void gDrawLine      ( DFBRegion    *line );
void gFillTriangle  ( DFBTriangle  *tri );

void gBlit          ( DFBRectangle *rect, int dx, int dy );
void gStretchBlit   ( DFBRectangle *srect, DFBRectangle *drect );

void gDrawString( const __u8 *text, int x, int y,
                  CoreFontData *font, CardState *state);

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
                        ((((pixel & 0x000003e0) * s1) +   (((g)<<2)* s)) & 0x00007c00)) >> 5;\
               *(d) = pixel;\
          }\
     }

#define SET_ALPHA_PIXEL_RGB16(d,r,g,b,a) \
     if (a) {\
          if ((a) == 0xff) {\
               *(d) = (((r)&0xf8) << 8) | (((g)&0xfc) << 3) | (((b)&0xf8) >>3);\
          }\
          else {\
               __u32 pixel = *(d);\
               __u8  s = ((a)>>2)+1;\
               __u8 s1 = 64-s;\
               \
               pixel = (((((pixel & 0x0000f81f) * s1) + (((((r)&0xf8)<<8) | ((b)>>3)) * s)) & 0x003e07c0) + \
                        ((((pixel & 0x000007e0) * s1) +   (((g)<<3) * s)) & 0x0001f800)) >> 6;\
               *(d) = pixel;\
          }\
     }

#define SET_ALPHA_PIXEL_RGB24(d,r,g,b,a)\
     { \
          __u8 dr, dg, db;\
          __u8 ar, ag, ab;\
          __u8 *dd = (d);\
          \
          db = *dd++;\
          dg = *dd++;\
          dr = *dd;\
          \
          ar = ((((a)+1)*(r))>>8)  + dr - ((((a)+1)*dr) >> 8);\
          ag = ((((a)+1)*(g))>>8)  + dg - ((((a)+1)*dg) >> 8);\
          ab = ((((a)+1)*(b))>>8)  + db - ((((a)+1)*db) >> 8);\
          dd = d;\
          *dd++ = ab;\
          *dd++ = ag;\
          *dd   = ar;\
     }

#define SET_ALPHA_PIXEL_RGB32(d,r,g,b,a)\
     { \
          __u32 pixel=*(d); \
          __u16 s=(a)+1;    \
     	  __u16 s1=(256-s); \
          \
     	  *D = (((((pixel & 0x00ff00ff) * s1) + ((((r)<<16) | (b)) * s)) & 0xff00ff00) + \
     	        ((((pixel & 0x0000ff00) * s1) + (((g)<<8) * s)) & 0x00ff0000)) >> 8; \
     }


#define SET_ALPHA_PIXEL_ARGB(d,r,g,b,a)\
     { \
          __u8  dr, dg, db, da; \
          __u8  ar, ag, ab, aa; \
          __u32 pixel = *(d);\
          \
          da = (pixel & 0xFF000000) >> 24;\
          dr = (pixel & 0x00FF0000) >> 16;\
          dg = (pixel & 0x0000FF00) >> 8;\
          db = (pixel & 0x000000FF);\
          ar = ((((a)+1)*(r))>>8)  + dr - ((((a)+1)*dr) >> 8);\
          ag = ((((a)+1)*(g))>>8)  + dg - ((((a)+1)*dg) >> 8);\
          ab = ((((a)+1)*(b))>>8)  + db - ((((a)+1)*db) >> 8);\
          aa = ((((a)+1)*(a))>>8)  + da - ((((a)+1)*da) >> 8);\
          pixel = (aa << 24) | (ar << 16) | (ag << 8) | (ab);\
          *(d) = pixel;\
     }

#endif
