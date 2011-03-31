/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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
#include <termios.h>
#include <unistd.h>
#include <linux/vt.h>

#include <directfb.h>

typedef struct {
     int fd0;                      /* file descriptor of /dev/tty0 */
     int fd;                       /* file descriptor of /dev/ttyN
                                      where N is the number of the allocated VT,
                                      may be equal to 'fd0' if VT allocation
                                      is disabled by "--no-vt-switch" */

     int num;                      /* number of vt where DirectFB runs */
     int prev;                     /* number of vt DirectFB was started from */

     int old_fb;                   /* original fb mapped to vt */

     struct sigaction sig_usr1;    /* previous signal handler for USR1 */
     struct sigaction sig_usr2;    /* previous signal handler for USR2 */

     struct vt_mode   vt_mode;     /* previous VT mode */

     DirectThread    *thread;
     pthread_mutex_t  lock;
     pthread_cond_t   wait;

     int              vt_sig;
     struct termios   old_ts;

     bool             flush;
     DirectThread    *flush_thread;
} VirtualTerminal;

/*
 * allocates and switches to a new virtual terminal
 */
DFBResult dfb_vt_initialize( void );
DFBResult dfb_vt_join( void );

/*
 * deallocates virtual terminal
 */
DFBResult dfb_vt_shutdown( bool emergency );
DFBResult dfb_vt_leave( bool emergency );

DFBResult dfb_vt_detach( bool force );

bool dfb_vt_switch( int num );

#endif
