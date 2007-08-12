/*
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

#ifndef __VMWARE_GFXDRIVER_H__
#define __VMWARE_GFXDRIVER_H__

#ifndef FB_ACCEL_VMWARE_BLITTER
#define FB_ACCEL_VMWARE_BLITTER 51
#endif


typedef struct {
     /* validation flags */
     int                    v_flags;

     /* cached/computed values */
     void                  *dst_addr;
     unsigned long          dst_pitch;
     DFBSurfacePixelFormat  dst_format;
     unsigned long          dst_bpp;

     void                  *src_addr;
     unsigned long          src_pitch;
     DFBSurfacePixelFormat  src_format;
     unsigned long          src_bpp;

     unsigned long          color_pixel;

     /** Add shared data here... **/
} VMWareDeviceData;


typedef struct {

     /** Add local data here... **/
} VMWareDriverData;


#endif
