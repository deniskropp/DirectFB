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
#include <pthread.h>

#include <core/fusion/shmalloc.h>

#include "directfb.h"
#include "directfb_internals.h"

#include "misc/util.h"
#include "misc/mem.h"

#include "core.h"
#include "coredefs.h"
#include "coretypes.h"
#include "gfxcard.h"
#include "vt.h"

/*
 *  FIXME: the following looks like a bad hack.
 *
 *  SIGUNUSED is no longer unused, but is defined for backwards compatibility.
 *  sparc, mips and alpha signal.h however do not define SIGUNUSED.
 */

#ifdef SIGUNUSED
#define SIG_SWITCH_FROM  (SIGUNUSED + 10)
#define SIG_SWITCH_TO    (SIGUNUSED + 11)
#else
#define SIG_SWITCH_FROM  (31 + 10)
#define SIG_SWITCH_TO    (31 + 11)
#endif

#ifndef SI_KERNEL
// glibc 2.1.x doesn't have this in /usr/include/bits/siginfo.h
#define SI_KERNEL 0x80
#endif

VirtualTerminal *core_vt = NULL;

static DFBResult vt_init_switching();


DFBResult vt_initialize()
{
     DFBResult ret;
     struct vt_stat vs;

     core_vt = (VirtualTerminal*)DFBCALLOC( 1, sizeof(VirtualTerminal) );
     Score_vt = (VirtualTerminalShared*)shmalloc( sizeof(VirtualTerminal) );

     setsid();
     core_vt->fd0 = open( "/dev/tty0", O_WRONLY );
     if (core_vt->fd0 < 0) {
          if (errno == ENOENT) {
               core_vt->fd0 = open( "/dev/vc/0", O_RDWR );
               if (core_vt->fd0 < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/core/vt: Couldn't open "
                                    "neither `/dev/tty0' nor `/dev/vc/0'!\n" );
                    }
                    else {
                         PERRORMSG( "DirectFB/core/vt: "
                                    "Error opening `/dev/vc/0'!\n" );
                    }

                    DFBFREE( core_vt );
                    core_vt = NULL;

                    return DFB_INIT;
               }
          }
          else {
               PERRORMSG( "DirectFB/core/vt: Error opening `/dev/tty0'!\n");

               DFBFREE( core_vt );
               core_vt = NULL;

               return DFB_INIT;
          }
     }

     if (ioctl( core_vt->fd0, VT_GETSTATE, &vs ) < 0) {
          PERRORMSG( "DirectFB/core/vt: VT_GETSTATE failed!\n" );
          close( core_vt->fd0 );
          DFBFREE( core_vt );
          core_vt = NULL;
          return DFB_INIT;
     }
     Score_vt->prev = vs.v_active;

     if (dfb_config->no_vt_switch) {
          core_vt->fd   = core_vt->fd0;
          Score_vt->num = Score_vt->prev;
     }
     else {
          int n;

          n = ioctl( core_vt->fd0, VT_OPENQRY, &Score_vt->num );
          if (n < 0 || Score_vt->num == -1) {
               PERRORMSG( "DirectFB/core/vt: Cannot allocate VT!\n" );
               close( core_vt->fd0 );
               DFBFREE( core_vt );
               core_vt = NULL;
               return DFB_INIT;
          }

          while (ioctl( core_vt->fd0, VT_ACTIVATE, Score_vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               PERRORMSG( "DirectFB/core/vt: VT_ACTIVATE failed!\n" );
               close( core_vt->fd0 );
               DFBFREE( core_vt );
               core_vt = NULL;
               return DFB_INIT;
          }

          while (ioctl( core_vt->fd0, VT_WAITACTIVE, Score_vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               PERRORMSG( "DirectFB/core/vt: VT_WAITACTIVE failed!\n" );
               close( core_vt->fd0 );
               DFBFREE( core_vt );
               core_vt = NULL;
               return DFB_INIT;
          }
     }

     ret = vt_init_switching();
     if (ret) {
          if (!dfb_config->no_vt_switch) {
               DEBUGMSG( "switching back...\n" );
               ioctl( core_vt->fd0, VT_ACTIVATE, Score_vt->prev );
               ioctl( core_vt->fd0, VT_WAITACTIVE, Score_vt->prev );
               DEBUGMSG( "...switched back\n" );
               ioctl( core_vt->fd0, VT_DISALLOCATE, Score_vt->num );
          }

          close( core_vt->fd0 );
          DFBFREE( core_vt );
          core_vt = NULL;
          return ret;
     }

     return DFB_OK;
}

DFBResult vt_join()
{
     return DFB_OK;
}

DFBResult vt_shutdown()
{
     if (dfb_config->vt_switching) {
          if (ioctl( core_vt->fd, VT_SETMODE, &Score_vt->vt_mode ) < 0)
               PERRORMSG( "DirectFB/core/vt: Unable to restore VT mode!!!\n" );

          sigaction( SIG_SWITCH_FROM, &Score_vt->sig_usr1, NULL );
          sigaction( SIG_SWITCH_TO, &Score_vt->sig_usr2, NULL );
     }

     if (!dfb_config->no_vt_switch) {
          DEBUGMSG( "switching back...\n" );

          if (ioctl( core_vt->fd0, VT_ACTIVATE, Score_vt->prev ) < 0)
               PERRORMSG( "DirectFB/core/vt: VT_ACTIVATE" );

          if (ioctl( core_vt->fd0, VT_WAITACTIVE, Score_vt->prev ) < 0)
               PERRORMSG( "DirectFB/core/vt: VT_WAITACTIVE" );

          DEBUGMSG( "switched back...\n" );

          if (close( core_vt->fd ) < 0)
               PERRORMSG( "DirectFB/core/vt: Unable to "
                          "close file descriptor of allocated VT!\n" );

          if (ioctl( core_vt->fd0, VT_DISALLOCATE, Score_vt->num ) < 0)
               PERRORMSG( "DirectFB/core/vt: Unable to disallocate VT!\n" );
     }

     if (close( core_vt->fd0 ) < 0)
          PERRORMSG( "DirectFB/core/vt: Unable to "
                     "close file descriptor of tty0!\n" );

     DFBFREE( core_vt );
     core_vt = NULL;

     return DFB_OK;
}

DFBResult vt_leave()
{
     return DFB_OK;
}

static DFBResult vt_init_switching()
{
     char buf[32];

     /* FIXME: Opening the device should be moved out of this function. */

     sprintf(buf, "/dev/tty%d", Score_vt->num);
     core_vt->fd = open( buf, O_RDWR );
     if (core_vt->fd < 0) {
          if (errno == ENOENT) {
               sprintf(buf, "/dev/vc/%d", Score_vt->num);
               core_vt->fd = open( buf, O_RDWR );
               if (core_vt->fd < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/core/vt: Couldn't open "
                                    "neither `/dev/tty%d' nor `/dev/vc/%d'!\n",
                                    Score_vt->num, Score_vt->num );
                    }
                    else {
                         PERRORMSG( "DirectFB/core/vt: "
                                    "Error opening `%s'!\n", buf );
                    }

                    return errno2dfb( errno );
               }
          }
          else {
               PERRORMSG( "DirectFB/core/vt: Error opening `%s'!\n", buf );
               return errno2dfb( errno );
          }
     }

     return DFB_OK;
}

