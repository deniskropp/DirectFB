/*
   TI Davinci driver

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org>

   Code is derived from VMWare driver.

   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

#ifndef __DAVINCI_GFXDRIVER_H__
#define __DAVINCI_GFXDRIVER_H__

#include <linux/fb.h>


typedef struct {
     /* validation flags */
     int                    v_flags;

     /* cached/computed values */
     void                  *dst_addr;
     unsigned long          dst_phys;
     unsigned long          dst_pitch;
     DFBSurfacePixelFormat  dst_format;
     unsigned long          dst_bpp;

     void                  *src_addr;
     unsigned long          src_phys;
     unsigned long          src_pitch;
     DFBSurfacePixelFormat  src_format;
     unsigned long          src_bpp;

     unsigned long          color_pixel;

     /** Add shared data here... **/
     struct fb_fix_screeninfo  fix[4];

     CoreSurfacePool          *osd_pool;
     CoreSurfacePool          *video_pool;
} DavinciDeviceData;


typedef struct {
     int                    fd;
     void                  *mem;
     int                    size;
} DavinciFB;

typedef struct {
     DavinciDeviceData     *ddev;

     CoreDFB               *core;

     CoreScreen            *screen;
     CoreLayer             *osd;
     CoreLayer             *video;

     DavinciFB              fb[4];
} DavinciDriverData;


#endif
