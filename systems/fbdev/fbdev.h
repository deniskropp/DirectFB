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

#ifndef __CORE__FBDEV_H__
#define __CORE__FBDEV_H__

#include <linux/fb.h>

#include <core/coretypes.h>

#include <core/system.h>

#include <fusion/call.h>
#include <fusion/reactor.h>

#include "vt.h"

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC	_IOW('F', 0x20, u_int32_t)
#endif


typedef struct {
     /* fbdev fixed screeninfo, contains infos about memory and type of card */
     struct fb_fix_screeninfo fix;

     VideoMode                *modes;        /* linked list of valid
                                                video modes */
     VideoMode                 current_mode; /* current video mode */

     struct fb_var_screeninfo current_var;   /* fbdev variable screeninfo
                                                set by DirectFB */
     struct fb_var_screeninfo orig_var;      /* fbdev variable screeninfo
                                                before DirectFB was started */
     struct fb_cmap           orig_cmap;     /* original palette */

     struct fb_cmap           current_cmap;  /* our copy of the cmap */

     struct fb_cmap           temp_cmap;     /* scratch */

     FusionCall               fbdev_ioctl;   /* ioctl rpc */

     unsigned long            page_mask;     /* PAGE_SIZE - 1 */
     
     struct {
          int                 bus;
          int                 dev;
          int                 func;
     } pci;                                  /* PCI Bus ID of graphics device */
     
     struct {
          unsigned short      vendor;        /* Graphics device vendor id */
          unsigned short      model;         /* Graphics device model id */
     } device;
} FBDevShared;

typedef struct {
     FBDevShared             *shared;

     CoreDFB                 *core;

     /* virtual framebuffer address */
     void                    *framebuffer_base;

     int                      fd;            /* file descriptor for /dev/fb */

     VirtualTerminal         *vt;
} FBDev;

/*
 * core init function, opens /dev/fb, get fbdev screeninfo
 * disables font acceleration, reads mode list
 */
DFBResult dfb_fbdev_initialize();
DFBResult dfb_fbdev_join();

/*
 * deinitializes DirectFB fbdev stuff and restores fbdev settings
 */
DFBResult dfb_fbdev_shutdown( bool emergency );
DFBResult dfb_fbdev_leave( bool emergency );

#endif
