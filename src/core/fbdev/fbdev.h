/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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
