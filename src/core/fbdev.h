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

#ifndef __CORE__FBDEV_H__
#define __CORE__FBDEV_H__

#include <linux/fb.h>

/*
 * hold information of a Videomode read from /etc/fb.modes
 * (to be replaced by DirectFB's own config system)
 */
typedef struct _VideoMode {
     int xres;
     int yres;
     int bpp;

     int pixclock;
     int left_margin;
     int right_margin;
     int upper_margin;
     int lower_margin;
     int hsync_len;
     int vsync_len;
     int hsync_high;
     int vsync_high;

     int laced;
     int doubled;

     DFBSurfacePixelFormat format; /* to be evaluated after mode switch */

     struct _VideoMode *next;
} VideoMode;


typedef struct {
     int                      fd;            /* file descriptor for /dev/fb */

     VideoMode                *modes;        /* linked list of valid
                                                video modes */
     VideoMode                *current_mode; /* current video mode */

     struct fb_var_screeninfo current_var;   /* fbdev variable screeninfo
                                                set by DirectFB */
     struct fb_var_screeninfo orig_var;      /* fbdev variable screeninfo
                                                before DirectFB was started */
} FBDev;

extern FBDev *fbdev;

/*
 * core init function, opens /dev/fb, get fbdev screeninfo
 * disables font acceleration, reads mode list
 */
DFBResult fbdev_open();

/*
 * return when vertical retrace is reached, works with matrox kernel patch
 * only for now
 */
DFBResult fbdev_wait_vsync();

DFBResult primarylayer_init();

#endif
