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

#include <unistd.h>
#include <malloc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/vt.h>
#include <errno.h>

#include "directfb.h"

#include "core.h"
#include "coredefs.h"

#include "vt.h"


VirtualTerminal     *vt = NULL;


/*
 * deallocates virtual terminal
 */
void vt_close()
{
     if (!vt) {
          BUG( "vt_close() called with none allocated!?" );
          return;
     }

     if (!dfb_config->no_vt_switch) {
          DEBUGMSG( "switching back...\n" );
          ioctl( vt->fd, VT_ACTIVATE, vt->prev );
          ioctl( vt->fd, VT_WAITACTIVE, vt->prev );
          DEBUGMSG( "switched back...\n" );
          ioctl( vt->fd, VT_DISALLOCATE, vt->num );
     }
     
     close( vt->fd );
     
     free( vt );
     vt = NULL;
}

DFBResult vt_open()
{
     int n;
     struct vt_stat vs;
     
     if (vt) {
          BUG( "vt_open() called again!" );
          return DFB_BUG;
     }

     vt = (VirtualTerminal*)malloc( sizeof(VirtualTerminal) );

     setsid();
     vt->fd = open( "/dev/tty0", O_WRONLY );
     if (vt->fd < 0) {
          if (errno == ENOENT) {
               vt->fd = open( "/dev/vc/0", O_RDWR );
               if (vt->fd < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/core/vt: Couldn't open "
                                    "neither `/dev/tty0' nor `/dev/vc/0'!\n" );
                    }
                    else {
                         PERRORMSG( "DirectFB/core/vt: "
                                    "Error opening `/dev/vc/0'!\n" );
                    }
     
                    free( vt );
                    vt = NULL;
     
                    return DFB_INIT;
               }
          }
          else {
               PERRORMSG( "DirectFB/core/vt: Error opening `/dev/tty0'!\n");
     
               free( vt );
               vt = NULL;
     
               return DFB_INIT;
          }
     }

     if (ioctl( vt->fd, VT_GETSTATE, &vs ) < 0) {
          PERRORMSG( "DirectFB/core/vt: VT_GETSTATE failed!\n" );
          close( vt->fd );
          free( vt );
          vt = NULL;
          return DFB_INIT;
     }
     vt->prev = vs.v_active;

     if (dfb_config->no_vt_switch) {
          vt->num = vt->prev;
     }
     else {
          n = ioctl( vt->fd, VT_OPENQRY, &vt->num );
          if (n < 0 || vt->num == -1) {
               PERRORMSG( "DirectFB/core/vt: Cannot allocate VT!\n" );
               close( vt->fd );
               free( vt );
               vt = NULL;
               return DFB_INIT;
          }

          while (ioctl( vt->fd, VT_ACTIVATE, vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               PERRORMSG( "DirectFB/core/vt: VT_ACTIVATE failed!\n" );
               close( vt->fd );
               free( vt );
               vt = NULL;
               return DFB_INIT;
          }

          while (ioctl( vt->fd, VT_WAITACTIVE, vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               PERRORMSG( "DirectFB/core/vt: VT_WAITACTIVE failed!\n" );
               close( vt->fd );
               free( vt );
               vt = NULL;
               return DFB_INIT;
          }
     }

     return DFB_OK;
}
