/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <directfb.h>

#include "convert.h"


DFBSurfacePixelFormat
dfb_pixelformat_for_depth( int depth )
{
     switch (depth) {
          case 8:
               return DSPF_LUT8;
          case 12:
               return DSPF_ARGB4444;
          case 14:
               return DSPF_ARGB2554;
          case 15:
               return DSPF_ARGB1555;
          case 16:
               return DSPF_RGB16;
          case 24:
               return DSPF_RGB24;
          case 32:
               return DSPF_RGB32;
     }

     return DSPF_UNKNOWN;
}

__u32
dfb_color_to_pixel( DFBSurfacePixelFormat format,
                    __u8 r, __u8 g, __u8 b )
{
     __u32 pixel;
     __u32 y, cb, cr;

     switch (format) {
          case DSPF_RGB332:
               pixel = PIXEL_RGB332( r, g, b );
               break;
          case DSPF_ARGB1555:
               pixel = PIXEL_ARGB1555( 0, r, g, b );
               break;
          case DSPF_ARGB2554:
               pixel = PIXEL_ARGB2554( 0, r, g, b );
               break;
          case DSPF_ARGB4444:
               pixel = PIXEL_ARGB4444( 0, r, g, b );
               break;
          case DSPF_RGB16:
               pixel = PIXEL_RGB16( r, g, b );
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_AiRGB:
               pixel = PIXEL_RGB32( r, g, b );
               break;
          case DSPF_AYUV:
               RGB_TO_YCBCR( r, g, b, y, cb, cr );
               pixel = PIXEL_AYUV( 0, y, cb, cr );
               break;
          case DSPF_YUY2:
               RGB_TO_YCBCR( r, g, b, y, cb, cr );
               pixel = PIXEL_YUY2( y, cb, cr );
               break;
          case DSPF_UYVY:
               RGB_TO_YCBCR( r, g, b, y, cb, cr );
               pixel = PIXEL_UYVY( y, cb, cr );
               break;
          case DSPF_I420:
          case DSPF_YV12:
               RGB_TO_YCBCR( r, g, b, y, cb, cr );
               pixel = y | (cb << 8) | (cr << 16);
               break;
          default:
               pixel = 0;
     }

     return pixel;
}

const char *
dfb_pixelformat_name( DFBSurfacePixelFormat format )
{
     switch (format) {
          case DSPF_UNKNOWN:
               return "UNKNOWN";

          case DSPF_ARGB1555:
               return "ARGB1555";

          case DSPF_RGB16:
               return "RGB16";

          case DSPF_RGB24:
               return "RGB24";

          case DSPF_RGB32:
               return "RGB32";

          case DSPF_ARGB:
               return "ARGB";

          case DSPF_A8:
               return "A8";

          case DSPF_YUY2:
               return "YUY2";

          case DSPF_RGB332:
               return "RGB332";

          case DSPF_UYVY:
               return "UYVY";

          case DSPF_I420:
               return "I420";

          case DSPF_YV12:
               return "YV12";

          case DSPF_LUT8:
               return "LUT8";

          case DSPF_ALUT44:
               return "ALUT44";

          case DSPF_AiRGB:
               return "AiRGB";

          case DSPF_A1:
               return "A1";

          case DSPF_NV12:
               return "NV12";

          case DSPF_NV21:
               return "NV21";

          case DSPF_NV16:
               return "NV16";

          case DSPF_ARGB2554:
               return "ARGB2554";

          case DSPF_ARGB4444:
               return "ARGB4444";

          case DSPF_AYUV:
               return "AYUV";
     }

     return "<invalid>";
}

