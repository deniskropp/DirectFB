/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by BoLiu <Bo@cirrus.com>

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
#ifndef __EP9X_H__
#define __EP9X_H__
#include <dfb_types.h>
#include <core/coretypes.h>
#include <core/layers.h>

/*
 * The following are the IOCTLs that can be sent to the EP93xx frame buffer
 * device.
 */
#define FBIO_EP9X_GET_CAPS    0x000046c0
#define FBIO_EP9X_CURSOR      0x000046c1
#define FBIO_EP9X_LINE        0x000046c2
#define FBIO_EP9X_FILL        0x000046c3
#define FBIO_EP9X_BLIT        0x000046c4
#define FBIO_EP9X_COPY        0x000046c5
#define FBIO_EP9X_GET_ADDR    0x000046c6

struct ep9x_line {
    __u32 flags;
    __s32 x1;
    __s32 y1;
    __s32 x2;
    __s32 y2;
    __u32 fgcolor;
    __u32 bgcolor;      // Only used if LINE_BACKGROUND is set
    __u32 pattern;      // Only used if LINE_PATTERN is set
};

/*
 * ioctl(fd, FBIO_EP93XX_FILL, ep93xx_fill *)
 *
 * Fills from dx to (dx + width - 1), and from dy to (dy + height - 1).
 */
struct ep9x_fill {
    __u32 dx;
    __u32 dy;
    __u32 width;
    __u32 height;
    __u32 color;
};


typedef struct {
     FBDev	*dfb_fbdev;
} EP9XDriverData;

typedef struct {
	unsigned long fb_addr;
	u32 fill_color;
	u32 pixelformat;
	u8  pixeldepth;
	bool fb_store;
	unsigned long srcaddr,destaddr,srcpitch,destpitch;
	DFBRegion clip;
     /* state validation */
        int smf_source;
        int smf_destination;
        int smf_color;
	int smf_clip;
} EP9XDeviceData;


#endif /*__EDB93XX_H__*/
