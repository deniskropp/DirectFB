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

#include <unistd.h>
#include <malloc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/vt.h>
#include <sys/kd.h>
#include <errno.h>
#include <pthread.h>

#include <core/fusion/shmalloc.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <misc/util.h>
#include <misc/mem.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/gfxcard.h>

#include <core/fbdev/vt.h>

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
     /* glibc 2.1.x doesn't have this in /usr/include/bits/siginfo.h */
     #define SI_KERNEL 0x80
#endif


VirtualTerminal *dfb_vt = NULL;

static DFBResult vt_init_switching();
static int       vt_get_fb( int vt );
static void      vt_set_fb( int vt, int fb );

DFBResult
dfb_vt_initialize()
{
     DFBResult ret;
     struct vt_stat vs;

     dfb_vt = (VirtualTerminal*)DFBCALLOC( 1, sizeof(VirtualTerminal) );
     Sdfb_vt = (VirtualTerminalShared*)shmalloc( sizeof(VirtualTerminal) );

     setsid();
     dfb_vt->fd0 = open( "/dev/tty0", O_WRONLY );
     if (dfb_vt->fd0 < 0) {
          if (errno == ENOENT) {
               dfb_vt->fd0 = open( "/dev/vc/0", O_RDWR );
               if (dfb_vt->fd0 < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/core/vt: Couldn't open "
                                    "neither `/dev/tty0' nor `/dev/vc/0'!\n" );
                    }
                    else {
                         PERRORMSG( "DirectFB/core/vt: "
                                    "Error opening `/dev/vc/0'!\n" );
                    }

                    DFBFREE( dfb_vt );
                    dfb_vt = NULL;

                    return DFB_INIT;
               }
          }
          else {
               PERRORMSG( "DirectFB/core/vt: Error opening `/dev/tty0'!\n");

               DFBFREE( dfb_vt );
               dfb_vt = NULL;

               return DFB_INIT;
          }
     }

     if (ioctl( dfb_vt->fd0, VT_GETSTATE, &vs ) < 0) {
          PERRORMSG( "DirectFB/core/vt: VT_GETSTATE failed!\n" );
          close( dfb_vt->fd0 );
          DFBFREE( dfb_vt );
          dfb_vt = NULL;
          return DFB_INIT;
     }

     Sdfb_vt->prev = vs.v_active;


     if (!dfb_config->vt_switch) {
          dfb_vt->fd   = dfb_vt->fd0;
          Sdfb_vt->num = Sdfb_vt->prev;

          /* move vt to framebuffer */
          Sdfb_vt->old_fb = vt_get_fb( Sdfb_vt->num );
          vt_set_fb( Sdfb_vt->num, -1 );
     }
     else {
          int n;

          n = ioctl( dfb_vt->fd0, VT_OPENQRY, &Sdfb_vt->num );
          if (n < 0 || Sdfb_vt->num == -1) {
               PERRORMSG( "DirectFB/core/vt: Cannot allocate VT!\n" );
               close( dfb_vt->fd0 );
               DFBFREE( dfb_vt );
               dfb_vt = NULL;
               return DFB_INIT;
          }

          /* move vt to framebuffer */
          Sdfb_vt->old_fb = vt_get_fb( Sdfb_vt->num );
          vt_set_fb( Sdfb_vt->num, -1 );

          /* switch to vt */
          while (ioctl( dfb_vt->fd0, VT_ACTIVATE, Sdfb_vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               PERRORMSG( "DirectFB/core/vt: VT_ACTIVATE failed!\n" );
               close( dfb_vt->fd0 );
               DFBFREE( dfb_vt );
               dfb_vt = NULL;
               return DFB_INIT;
          }

          while (ioctl( dfb_vt->fd0, VT_WAITACTIVE, Sdfb_vt->num ) < 0) {
               if (errno == EINTR)
                    continue;
               PERRORMSG( "DirectFB/core/vt: VT_WAITACTIVE failed!\n" );
               close( dfb_vt->fd0 );
               DFBFREE( dfb_vt );
               dfb_vt = NULL;
               return DFB_INIT;
          }

          usleep( 40000 );
     }

     ret = vt_init_switching();
     if (ret) {
          if (dfb_config->vt_switch) {
               DEBUGMSG( "switching back...\n" );
               ioctl( dfb_vt->fd0, VT_ACTIVATE, Sdfb_vt->prev );
               ioctl( dfb_vt->fd0, VT_WAITACTIVE, Sdfb_vt->prev );
               DEBUGMSG( "...switched back\n" );
               ioctl( dfb_vt->fd0, VT_DISALLOCATE, Sdfb_vt->num );
          }

          close( dfb_vt->fd0 );
          DFBFREE( dfb_vt );
          dfb_vt = NULL;
          return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_vt_join()
{
     return DFB_OK;
}

DFBResult
dfb_vt_shutdown( bool emergency )
{
     if (!dfb_vt)
          return DFB_OK;
#if 0
     if (dfb_config->vt_switching) {
          if (ioctl( dfb_vt->fd, VT_SETMODE, &Sdfb_vt->vt_mode ) < 0)
               PERRORMSG( "DirectFB/core/vt: Unable to restore VT mode!!!\n" );

          //sigaction( SIG_SWITCH_FROM, &Sdfb_vt->sig_usr1, NULL );
          //sigaction( SIG_SWITCH_TO, &Sdfb_vt->sig_usr2, NULL );
     }
#endif
     if (dfb_config->vt_switch) {
          DEBUGMSG( "switching back...\n" );

          if (ioctl( dfb_vt->fd0, VT_ACTIVATE, Sdfb_vt->prev ) < 0)
               PERRORMSG( "DirectFB/core/vt: VT_ACTIVATE" );

          if (ioctl( dfb_vt->fd0, VT_WAITACTIVE, Sdfb_vt->prev ) < 0)
               PERRORMSG( "DirectFB/core/vt: VT_WAITACTIVE" );

          DEBUGMSG( "switched back...\n" );

          usleep( 40000 );

          /* restore con2fbmap */
          vt_set_fb( Sdfb_vt->num, Sdfb_vt->old_fb );

          if (close( dfb_vt->fd ) < 0)
               PERRORMSG( "DirectFB/core/vt: Unable to "
                          "close file descriptor of allocated VT!\n" );

          if (ioctl( dfb_vt->fd0, VT_DISALLOCATE, Sdfb_vt->num ) < 0)
               PERRORMSG( "DirectFB/core/vt: Unable to disallocate VT!\n" );
     }
     else {
          /* restore con2fbmap */
          vt_set_fb( Sdfb_vt->num, Sdfb_vt->old_fb );
     }

     if (close( dfb_vt->fd0 ) < 0)
          PERRORMSG( "DirectFB/core/vt: Unable to "
                     "close file descriptor of tty0!\n" );

     DFBFREE( dfb_vt );
     dfb_vt = NULL;

     return DFB_OK;
}

DFBResult
dfb_vt_leave( bool emergency )
{
     return DFB_OK;
}

static DFBResult
vt_init_switching()
{
     char buf[32];

     /* FIXME: Opening the device should be moved out of this function. */

     snprintf(buf, 32, "/dev/tty%d", Sdfb_vt->num);
     dfb_vt->fd = open( buf, O_RDWR );
     if (dfb_vt->fd < 0) {
          if (errno == ENOENT) {
               snprintf(buf, 32, "/dev/vc/%d", Sdfb_vt->num);
               dfb_vt->fd = open( buf, O_RDWR );
               if (dfb_vt->fd < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/core/vt: Couldn't open "
                                    "neither `/dev/tty%d' nor `/dev/vc/%d'!\n",
                                    Sdfb_vt->num, Sdfb_vt->num );
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

     if (dfb_config->kd_graphics) {
          if (ioctl( dfb_vt->fd, KDSETMODE, KD_GRAPHICS ) < 0) {
               PERRORMSG( "DirectFB/Keyboard: KD_GRAPHICS failed!\n" );
               close( dfb_vt->fd );
               return DFB_INIT;
          }
     }

     if (dfb_config->vt_switch) {
          if (ioctl( dfb_vt->fd0, TIOCNOTTY, 0 ) < 0);
/*               PERRORMSG( "DirectFB/Keyboard: TIOCNOTTY failed!\n" );*/
          
          if (ioctl( dfb_vt->fd, TIOCSCTTY, 0 ) < 0);
/*               PERRORMSG( "DirectFB/Keyboard: TIOCSCTTY failed!\n" );*/
     }
     
     return DFB_OK;
}

static int
vt_get_fb( int vt )
{
     int                  fd;
     struct fb_con2fbmap  c2m;
     const char          *fbpath = "/dev/fb0";

     if (dfb_config->fb_device)
          fbpath = dfb_config->fb_device;

     c2m.console = vt;

     fd = open( fbpath, O_RDWR );
     if (fd != -1) {
          if (ioctl( fd, FBIOGET_CON2FBMAP, &c2m ) < 0) {
               PERRORMSG( "DirectFB/core/fbdev: "
                          "FBIOGET_CON2FBMAP failed!\n" );
          }

          close( fd );
     }

     return c2m.framebuffer;
}

static void
vt_set_fb( int vt, int fb )
{
     int                  fd;
     struct fb_con2fbmap  c2m;
     const char          *fbpath = "/dev/fb0";

     if (dfb_config->fb_device) {
          struct stat sbf;

          if (!stat( dfb_config->fb_device, &sbf )) {
               fbpath = dfb_config->fb_device;
               c2m.framebuffer = (sbf.st_rdev & 0xFF) >> 5;
          }
     }
     else
          c2m.framebuffer = 0;

     if (fb > -1)
          c2m.framebuffer = fb;

     c2m.console = vt;

     fd = open( fbpath, O_RDWR );
     if (fd != -1) {
          if (ioctl( fd, FBIOPUT_CON2FBMAP, &c2m ) < 0) {
               PERRORMSG( "DirectFB/core/fbdev: "
                          "FBIOPUT_CON2FBMAP failed!\n" );
          }

          close( fd );
     }
}

