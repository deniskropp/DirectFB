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

/*
 * deallocates virtual terminal
 */
void vt_close()
{
     if (!core_vt) {
          BUG( "vt_close() multiple times" );
          return;
     }

     if (dfb_config->vt_switching) {
          if (ioctl( core_vt->fd, VT_SETMODE, &core_vt->vt_mode ) < 0)
               PERRORMSG( "DirectFB/core/vt: Unable to restore VT mode!!!\n" );

          sigaction( SIG_SWITCH_FROM, &core_vt->sig_usr1, NULL );
          sigaction( SIG_SWITCH_TO, &core_vt->sig_usr2, NULL );
     }

     if (!dfb_config->no_vt_switch) {
          DEBUGMSG( "switching back...\n" );

          if (ioctl( core_vt->fd0, VT_ACTIVATE, core_vt->prev ) < 0)
               PERRORMSG( "DirectFB/core/vt: VT_ACTIVATE" );

          if (ioctl( core_vt->fd0, VT_WAITACTIVE, core_vt->prev ) < 0)
               PERRORMSG( "DirectFB/core/vt: VT_WAITACTIVE" );

          DEBUGMSG( "switched back...\n" );

          if (close( core_vt->fd ) < 0)
               PERRORMSG( "DirectFB/core/vt: Unable to "
                          "close file descriptor of allocated VT!\n" );

          if (ioctl( core_vt->fd0, VT_DISALLOCATE, core_vt->num ) < 0)
               PERRORMSG( "DirectFB/core/vt: Unable to disallocate VT!\n" );
     }

     if (close( core_vt->fd0 ) < 0)
          PERRORMSG( "DirectFB/core/vt: Unable to "
                     "close file descriptor of tty0!\n" );

     DFBFREE( core_vt );
     core_vt = NULL;
}

DFBResult vt_open()
{
     DFBResult ret;
     struct vt_stat vs;

     if (core_vt) {
          BUG( "vt_open() called again!" );
          return DFB_BUG;
     }

     core_vt = (VirtualTerminal*)DFBMALLOC( sizeof(VirtualTerminal) );

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
     core_vt->prev = vs.v_active;

     if (dfb_config->no_vt_switch) {
          core_vt->fd  = core_vt->fd0;
          core_vt->num = core_vt->prev;
     }
     else {
          int n;

          n = ioctl( core_vt->fd0, VT_OPENQRY, &core_vt->num );
          if (n < 0 || core_vt->num == -1) {
               PERRORMSG( "DirectFB/core/vt: Cannot allocate VT!\n" );
               close( core_vt->fd0 );
               DFBFREE( core_vt );
               core_vt = NULL;
               return DFB_INIT;
          }

          while (ioctl( core_vt->fd0, VT_ACTIVATE, core_vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               PERRORMSG( "DirectFB/core/vt: VT_ACTIVATE failed!\n" );
               close( core_vt->fd0 );
               DFBFREE( core_vt );
               core_vt = NULL;
               return DFB_INIT;
          }

          while (ioctl( core_vt->fd0, VT_WAITACTIVE, core_vt->num ) < 0) {
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
               ioctl( core_vt->fd0, VT_ACTIVATE, core_vt->prev );
               ioctl( core_vt->fd0, VT_WAITACTIVE, core_vt->prev );
               DEBUGMSG( "...switched back\n" );
               ioctl( core_vt->fd0, VT_DISALLOCATE, core_vt->num );
          }

          close( core_vt->fd0 );
          DFBFREE( core_vt );
          core_vt = NULL;
          return ret;
     }

     return DFB_OK;
}

static void vt_switch( int signal, siginfo_t *info, void *p )
{
     static pthread_mutex_t switch_lock = PTHREAD_MUTEX_INITIALIZER;
     static pthread_cond_t  switch_cond = PTHREAD_COND_INITIALIZER;

     DEBUGMSG( "DirectFB/core/vt: "
               "vt_switch signal handler activated (%d)\n", getpid() );

     if (info->si_code != SI_KERNEL) {
          DEBUGMSG( "DirectFB/core/vt: vt_switch signal handler aborting "
                    "(si_code != SI_KERNEL)\n" );

          DEBUGMSG( "DirectFB/core/vt: "
                    "going to call previous signal handler...\n" );

          if (signal == SIG_SWITCH_FROM) {
               if (core_vt->sig_usr1.sa_flags & SA_SIGINFO)
                    core_vt->sig_usr1.sa_sigaction( signal, info, p );
               else
                    core_vt->sig_usr1.sa_handler( signal );
          }
          else if (signal == SIG_SWITCH_TO) {
               if (core_vt->sig_usr2.sa_flags & SA_SIGINFO)
                    core_vt->sig_usr2.sa_sigaction( signal, info, p );
               else
                    core_vt->sig_usr2.sa_handler( signal );
          }

          DEBUGMSG( "DirectFB/core/vt: "
                    "...previous signal handler returned.\n" );

          return;
     }

     switch (signal) {
          case SIG_SWITCH_FROM:
               DEBUGMSG( "DirectFB/core/vt: Locking hardware...\n" );

               if (pthread_mutex_trylock( &card->lock )) {
                    DEBUGMSG( "DirectFB/core/vt: "
                              "Not allowing VT switch, hardware is in use!\n" );
                    if (ioctl( core_vt->fd, VT_RELDISP, 0 ) < 0)
                         PERRORMSG( "DirectFB/core/vt: "
                                    "Unable to disallow switch-from!\n" );
                    return;
               }

               gfxcard_sync();

               idirectfb_singleton->Suspend( idirectfb_singleton );

               DEBUGMSG( "DirectFB/core/vt: Releasing VT...\n" );

               if (ioctl( core_vt->fd, VT_RELDISP, 1 ) < 0) {
                    PERRORMSG( "DirectFB/core/vt: "
                               "Unable to allow switch-from!\n" );
               }
               else {
                    DEBUGMSG( "DirectFB/core/vt: ...VT released.\n" );

                    pthread_mutex_lock( &switch_lock );
                    DEBUGMSG( "DirectFB/core/vt: Chilling until acquisition...\n" );
                    pthread_cond_wait( &switch_cond, &switch_lock );
                    DEBUGMSG( "DirectFB/core/vt: ...chilled out.\n" );
                    pthread_mutex_unlock( &switch_lock );
               }

               pthread_mutex_unlock( &card->lock );

               DEBUGMSG( "DirectFB/core/vt: ...hardware unlocked.\n" );

               idirectfb_singleton->Resume( idirectfb_singleton );

               break;

          case SIG_SWITCH_TO:
               DEBUGMSG( "DirectFB/core/vt: Acquiring VT...\n" );

               if (ioctl( core_vt->fd, VT_RELDISP, 2 ) < 0) {
                    PERRORMSG( "DirectFB/core/vt: "
                               "Unable to allow switch-to!\n" );
               }
               else {
                    DEBUGMSG( "DirectFB/core/vt: ...VT acquired.\n" );

                    pthread_mutex_lock( &switch_lock );
                    DEBUGMSG( "DirectFB/core/vt: Kicking chillers' ass...\n" );
                    pthread_cond_broadcast( &switch_cond );
                    DEBUGMSG( "DirectFB/core/vt: ...chillers kick ass.\n" );
                    pthread_mutex_unlock( &switch_lock );
               }

               break;

          default:
               BUG( "signal != SIG_SWITCH_FROM or SIG_SWITCH_TO" );
     }

     DEBUGMSG( "DirectFB/core/vt: "
               "Returning from signal handler (%d)\n", getpid() );
}

static DFBResult vt_init_switching()
{
     struct sigaction sig_tty;
     struct vt_mode   vt_mode;

     char buf[32];

     /* FIXME: Opening the device should be moved out of this function. */

     sprintf(buf, "/dev/tty%d", core_vt->num);
     core_vt->fd = open( buf, O_RDWR );
     if (core_vt->fd < 0) {
          if (errno == ENOENT) {
               sprintf(buf, "/dev/vc/%d", core_vt->num);
               core_vt->fd = open( buf, O_RDWR );
               if (core_vt->fd < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/core/vt: Couldn't open "
                                    "neither `/dev/tty%d' nor `/dev/vc/%d'!\n",
                                    core_vt->num, core_vt->num );
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

     if (dfb_config->vt_switching) {
          memset( &sig_tty, 0, sizeof( sig_tty ) );
          sig_tty.sa_sigaction = vt_switch;
          sig_tty.sa_flags     = SA_SIGINFO | SA_RESTART;
          sigemptyset( &sig_tty.sa_mask );
          if (sigaction( SIG_SWITCH_FROM, &sig_tty, &core_vt->sig_usr1 ) ||
              sigaction( SIG_SWITCH_TO, &sig_tty, &core_vt->sig_usr2 ))
          {
               PERRORMSG( "DirectFB/core/vt: sigaction failed!\n" );
               return errno2dfb( errno );
          }

          if (ioctl( core_vt->fd, VT_GETMODE, &vt_mode ) < 0) {
               int erno = errno;

               PERRORMSG( "DirectFB/core/vt: VT_GETMODE failed!\n" );

               sigaction( SIG_SWITCH_FROM, &core_vt->sig_usr1, NULL );
               sigaction( SIG_SWITCH_TO, &core_vt->sig_usr2, NULL );

               return errno2dfb( erno );
          }

          core_vt->vt_mode = vt_mode;

          vt_mode.mode   = VT_PROCESS;
          vt_mode.waitv  = 0;
          vt_mode.relsig = SIG_SWITCH_FROM;
          vt_mode.acqsig = SIG_SWITCH_TO;

          if (ioctl( core_vt->fd, VT_SETMODE, &vt_mode) < 0) {
               int erno = errno;

               PERRORMSG( "DirectFB/core/vt: VT_SETMODE failed!\n" );

               sigaction( SIG_SWITCH_FROM, &core_vt->sig_usr1, NULL );
               sigaction( SIG_SWITCH_TO, &core_vt->sig_usr2, NULL );

               return errno2dfb( erno );
          }
     }

     return DFB_OK;
}

