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
dfb_color_to_aycbcr( const DFBColor *color )
{
     unsigned int red   = color->r;
     unsigned int green = color->g;
     unsigned int blue  = color->b;

     __u8 y  = (__u8)(((66 * red + 129 * green + 25 * blue) / 256) + 16);

     __u8 cb = (__u8)((128 * 256 -  38 * red   - 74 * green + 112 * blue) / 256);
     __u8 cr = (__u8)((128 * 256 + 112 * red   - 94 * green -  18 * blue) / 256);

     return (color->a << 24) | (y << 16) | (cb << 8) | cr;
}

__u32
dfb_color_to_pixel( DFBSurfacePixelFormat format,
                    __u8 r, __u8 g, __u8 b )
{
     __u32 pixel;

     switch (format) {
          case DSPF_RGB332:
               pixel = PIXEL_RGB332( r, g, b );
               break;
          case DSPF_ARGB1555:
               pixel = PIXEL_ARGB1555( 0, r, g, b );
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
          default:
               pixel = 0;
     }

     return pixel;
}

