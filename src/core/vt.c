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

VirtualTerminal *vt = NULL;

static DFBResult vt_init_switching();

/*
 * deallocates virtual terminal
 */
void vt_close()
{
     if (!vt) {
          BUG( "vt_close() multiple times" );
          return;
     }

     if (dfb_config->vt_switching) {
          if (ioctl( vt->fd, VT_SETMODE, &vt->vt_mode ) < 0)
               PERRORMSG( "DirectFB/core/vt: Unable to restore VT mode!!!\n" );

          sigaction( SIG_SWITCH_FROM, &vt->sig_usr1, NULL );
          sigaction( SIG_SWITCH_TO, &vt->sig_usr2, NULL );
     }

     if (!dfb_config->no_vt_switch) {
          DEBUGMSG( "switching back...\n" );
          ioctl( vt->fd0, VT_ACTIVATE, vt->prev );
          ioctl( vt->fd0, VT_WAITACTIVE, vt->prev );
          DEBUGMSG( "switched back...\n" );
          ioctl( vt->fd0, VT_DISALLOCATE, vt->num );

          close( vt->fd );
     }

     close( vt->fd0 );

     DFBFREE( vt );
     vt = NULL;
}

DFBResult vt_open()
{
     DFBResult ret;
     struct vt_stat vs;

     if (vt) {
          BUG( "vt_open() called again!" );
          return DFB_BUG;
     }

     vt = (VirtualTerminal*)DFBMALLOC( sizeof(VirtualTerminal) );

     setsid();
     vt->fd0 = open( "/dev/tty0", O_WRONLY );
     if (vt->fd0 < 0) {
          if (errno == ENOENT) {
               vt->fd0 = open( "/dev/vc/0", O_RDWR );
               if (vt->fd0 < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/core/vt: Couldn't open "
                                    "neither `/dev/tty0' nor `/dev/vc/0'!\n" );
                    }
                    else {
                         PERRORMSG( "DirectFB/core/vt: "
                                    "Error opening `/dev/vc/0'!\n" );
                    }

                    DFBFREE( vt );
                    vt = NULL;

                    return DFB_INIT;
               }
          }
          else {
               PERRORMSG( "DirectFB/core/vt: Error opening `/dev/tty0'!\n");

               DFBFREE( vt );
               vt = NULL;

               return DFB_INIT;
          }
     }

     if (ioctl( vt->fd0, VT_GETSTATE, &vs ) < 0) {
          PERRORMSG( "DirectFB/core/vt: VT_GETSTATE failed!\n" );
          close( vt->fd0 );
          DFBFREE( vt );
          vt = NULL;
          return DFB_INIT;
     }
     vt->prev = vs.v_active;

     if (dfb_config->no_vt_switch) {
          vt->fd  = vt->fd0;
          vt->num = vt->prev;
     }
     else {
          int n;

          n = ioctl( vt->fd0, VT_OPENQRY, &vt->num );
          if (n < 0 || vt->num == -1) {
               PERRORMSG( "DirectFB/core/vt: Cannot allocate VT!\n" );
               close( vt->fd0 );
               DFBFREE( vt );
               vt = NULL;
               return DFB_INIT;
          }

          while (ioctl( vt->fd0, VT_ACTIVATE, vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               PERRORMSG( "DirectFB/core/vt: VT_ACTIVATE failed!\n" );
               close( vt->fd0 );
               DFBFREE( vt );
               vt = NULL;
               return DFB_INIT;
          }

          while (ioctl( vt->fd0, VT_WAITACTIVE, vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               PERRORMSG( "DirectFB/core/vt: VT_WAITACTIVE failed!\n" );
               close( vt->fd0 );
               DFBFREE( vt );
               vt = NULL;
               return DFB_INIT;
          }
     }

     ret = vt_init_switching();
     if (ret) {
          if (!dfb_config->no_vt_switch) {
               DEBUGMSG( "switching back...\n" );
               ioctl( vt->fd0, VT_ACTIVATE, vt->prev );
               ioctl( vt->fd0, VT_WAITACTIVE, vt->prev );
               DEBUGMSG( "...switched back\n" );
               ioctl( vt->fd0, VT_DISALLOCATE, vt->num );
          }

          close( vt->fd0 );
          DFBFREE( vt );
          vt = NULL;
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
               if (vt->sig_usr1.sa_flags & SA_SIGINFO)
                    vt->sig_usr1.sa_sigaction( signal, info, p );
               else
                    vt->sig_usr1.sa_handler( signal );
          }
          else if (signal == SIG_SWITCH_TO) {
               if (vt->sig_usr2.sa_flags & SA_SIGINFO)
                    vt->sig_usr2.sa_sigaction( signal, info, p );
               else
                    vt->sig_usr2.sa_handler( signal );
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
                    if (ioctl( vt->fd, VT_RELDISP, 0 ) < 0)
                         PERRORMSG( "DirectFB/core/vt: "
                                    "Unable to disallow switch-from!\n" );
                    return;
               }

               gfxcard_sync();

               idirectfb_singleton->Suspend( idirectfb_singleton );

               DEBUGMSG( "DirectFB/core/vt: Releasing VT...\n" );

               if (ioctl( vt->fd, VT_RELDISP, 1 ) < 0) {
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

               if (ioctl( vt->fd, VT_RELDISP, 2 ) < 0) {
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

     sprintf(buf, "/dev/tty%d", vt->num);
     vt->fd = open( buf, O_RDWR );
     if (vt->fd < 0) {
          if (errno == ENOENT) {
               sprintf(buf, "/dev/vc/%d", vt->num);
               vt->fd = open( buf, O_RDWR );
               if (vt->fd < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/core/vt: Couldn't open "
                                    "neither `/dev/tty%d' nor `/dev/vc/%d'!\n",
                                    vt->num, vt->num );
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
          if (sigaction( SIG_SWITCH_FROM, &sig_tty, &vt->sig_usr1 ) ||
              sigaction( SIG_SWITCH_TO, &sig_tty, &vt->sig_usr2 ))
          {
               PERRORMSG( "DirectFB/core/vt: sigaction failed!\n" );
               return errno2dfb( errno );
          }

          if (ioctl( vt->fd, VT_GETMODE, &vt_mode ) < 0) {
               int erno = errno;

               PERRORMSG( "DirectFB/core/vt: VT_GETMODE failed!\n" );

               sigaction( SIG_SWITCH_FROM, &vt->sig_usr1, NULL );
               sigaction( SIG_SWITCH_TO, &vt->sig_usr2, NULL );

               return errno2dfb( erno );
          }

          vt->vt_mode = vt_mode;

          vt_mode.mode   = VT_PROCESS;
          vt_mode.waitv  = 0;
          vt_mode.relsig = SIG_SWITCH_FROM;
          vt_mode.acqsig = SIG_SWITCH_TO;

          if (ioctl( vt->fd, VT_SETMODE, &vt_mode) < 0) {
               int erno = errno;

               PERRORMSG( "DirectFB/core/vt: VT_SETMODE failed!\n" );

               sigaction( SIG_SWITCH_FROM, &vt->sig_usr1, NULL );
               sigaction( SIG_SWITCH_TO, &vt->sig_usr2, NULL );

               return errno2dfb( erno );
          }
     }

     return DFB_OK;
}

