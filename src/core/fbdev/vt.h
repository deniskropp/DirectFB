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

#ifndef __VT_H__
#define __VT_H__

#include <signal.h>
#include <linux/vt.h>

#include <directfb.h>

typedef struct _VirtualTerminalShared   VirtualTerminalShared;

struct _VirtualTerminalShared {
     int num;                      /* number of vt where DirectFB runs */
     int prev;                     /* number of vt DirectFB was started from */

     int old_fb;                   /* original fb mapped to vt */

     struct sigaction sig_usr1;    /* previous signal handler for USR1 */
     struct sigaction sig_usr2;    /* previous signal handler for USR2 */

     struct vt_mode   vt_mode;     /* previous VT mode */
};

struct _VirtualTerminal {
     VirtualTerminalShared *shared;

     int fd0;                      /* file descriptor of /dev/tty0 */
     int fd;                       /* file descriptor of /dev/ttyN
                                      where N is the number of the allocated VT,
                                      may be equal to 'fd0' if VT allocation
                                      is disabled by "--no-vt-switch" */
};

extern VirtualTerminal   *dfb_vt;

#define Sdfb_vt (dfb_vt->shared)

/*
 * allocates and switches to a new virtual terminal
 */
DFBResult dfb_vt_initialize();
DFBResult dfb_vt_join();

/*
 * deallocates virtual terminal
 */
DFBResult dfb_vt_shutdown( bool emergency );
DFBResult dfb_vt_leave( bool emergency );

#endif
